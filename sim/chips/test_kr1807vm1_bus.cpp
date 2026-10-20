// The bus side of КР1807ВМ1: the transactions on its pins, the edges
// measured against the AC table of the T-11 Engineering Specification
// (docs/t11.md §5.8) at T = 133 ns, as docs/t11-model.md places them.
#include <doctest/doctest.h>
#include <verilated.h>

#include "Vkr1807vm1_bus.h"

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace {

constexpr int kTicksPerT = 16;                  // 120 MHz master, 7.5 MHz XTL
constexpr double kTick = 1000.0 / 120.0;        // ns
constexpr double kT = kTick * kTicksPerT;

enum Kind { NOP = 0, READ = 1, WRITE = 2, ASPI = 3, IACK = 4 };

struct Request {
    Kind kind;
    std::uint16_t addr = 0, data = 0;
    int bytes = 3;
    bool fetch = false;
    int ack = 0;
};

// The pins at one master tick.
struct Sample {
    bool ras_n, cas_n, pi, cout, sel0, sel1, rwhb, rwlb;
    std::uint16_t dal_oe, dal_out;
};

class Bench {
public:
    explicit Bench(std::vector<Request> script, bool delayed = false, bool pulse = true)
        : script_(std::move(script))
    {
        m_.delayed = delayed;
        m_.cout_pulse = pulse;
        for (int i = 0; i < 2 * kTicksPerT + 3; ++i)    // idle, then start off phase
            tick();
        m_.run = 1;
        present(0);
        while (taken_ <= script_.size() && t_ < 100000)
            tick();
    }

    std::vector<Sample> s;
    std::vector<int> takes;                     // tick of each transaction start
    std::map<std::uint16_t, std::uint16_t> memory{{0172000, 012700}, {01000, 0x1234}};
    std::vector<std::uint16_t> read_data;
    std::vector<int> ai_seen;
    std::vector<std::uint16_t> vectors;

private:
    void present(std::size_t i)
    {
        const Request r = i < script_.size() ? script_[i] : Request{NOP};
        m_.req_kind = static_cast<std::uint8_t>(r.kind);
        m_.req_addr = r.addr;
        m_.req_data = r.data;
        m_.req_bytes = static_cast<std::uint8_t>(r.bytes);
        m_.req_fetch = r.fetch;
        m_.req_ack = static_cast<std::uint8_t>(r.ack);
    }

    // A memory on the pins: the address at -RAS falling, a read driven
    // while -CAS is low and the processor lets DAL go, a write taken while
    // -CAS and a byte strobe are low; an IACK's vector while SEL1 is high.
    void device()
    {
        if (!m_.ras_n && ras_was_)
            address_ = m_.dal_out;
        std::uint16_t in = 0xFFFF;
        if (!m_.ras_n && !m_.cas_n && m_.dal_oe == 0)
            in = memory[address_];
        if (m_.sel1)
            in = 0x0134;
        if (!m_.cas_n && !(m_.rwhb && m_.rwlb)) {
            std::uint16_t& w = memory[address_];
            if (!m_.rwlb) w = static_cast<std::uint16_t>((w & 0xFF00) | (m_.dal_out & 0x00FF));
            if (!m_.rwhb) w = static_cast<std::uint16_t>((w & 0x00FF) | (m_.dal_out & 0xFF00));
        }
        m_.dal_in = in;
        m_.ai_in = 0xA5;
        ras_was_ = m_.ras_n;
        cas_was_ = m_.cas_n;
    }

    void tick()
    {
        m_.xtl = (t_ % kTicksPerT) < kTicksPerT / 2 ? 1 : 0;
        device();
        m_.clk = 0;
        m_.eval();
        m_.clk = 1;
        m_.eval();
        ++t_;
        s.push_back({m_.ras_n != 0, m_.cas_n != 0, m_.pi != 0, m_.cout != 0, m_.sel0 != 0,
                     m_.sel1 != 0, m_.rwhb != 0, m_.rwlb != 0, m_.dal_oe, m_.dal_out});
        if (m_.take) {
            takes.push_back(static_cast<int>(s.size()) - 1);
            present(++taken_);
        }
        if (m_.data_valid) read_data.push_back(m_.data);
        if (m_.ai_valid) ai_seen.push_back(m_.ai);
        if (m_.vector_valid) vectors.push_back(m_.data);
    }

    std::unique_ptr<Vkr1807vm1_bus> model_ = std::make_unique<Vkr1807vm1_bus>();
    Vkr1807vm1_bus& m_ = *model_;
    std::vector<Request> script_;
    std::size_t taken_ = 0;
    std::uint64_t t_ = 0;
    std::uint16_t address_ = 0;
    bool ras_was_ = true, cas_was_ = true;
};

using Pin = bool Sample::*;

// The first tick at or after `from` where `pin` has become `level`.
int edge(const Bench& b, int from, Pin pin, bool level)
{
    for (int t = from; t < static_cast<int>(b.s.size()); ++t)
        if (b.s[static_cast<std::size_t>(t)].*pin == level &&
            (t == 0 || b.s[static_cast<std::size_t>(t) - 1].*pin != level))
            return t;
    return -1;
}

// The ticks of one read or write transaction's edges.
struct Edges {
    int addr, ras_fall, cas_fall, pi_rise, cas_rise, pi_fall, ras_rise, next;
    int cout_rise1, cout_fall1, cout_rise2, cout_fall2;
};

Edges edges_of(const Bench& b, std::size_t i)
{
    Edges e{};
    e.addr = b.takes[i];
    e.next = b.takes[i + 1];
    e.ras_fall = edge(b, e.addr, &Sample::ras_n, false);
    e.cas_fall = edge(b, e.addr, &Sample::cas_n, false);
    e.pi_rise = edge(b, e.addr, &Sample::pi, true);
    e.cas_rise = edge(b, e.cas_fall, &Sample::cas_n, true);
    e.pi_fall = edge(b, e.pi_rise, &Sample::pi, false);
    e.ras_rise = edge(b, e.ras_fall, &Sample::ras_n, true);
    e.cout_rise1 = edge(b, e.addr, &Sample::cout, true);
    e.cout_fall1 = edge(b, e.cout_rise1, &Sample::cout, false);
    e.cout_rise2 = edge(b, e.cout_fall1, &Sample::cout, true);
    e.cout_fall2 = edge(b, e.cout_rise2, &Sample::cout, false);
    return e;
}

double ns(int from, int to) { return (to - from) * kTick; }

// at least `min` ns from `from` to `to`
#define AT_LEAST(name, from, to, min) \
    CHECK_MESSAGE(ns(from, to) >= (min) - 0.01, name << ": " << ns(from, to) << " < " << (min))
#define AT_MOST(name, from, to, max) \
    CHECK_MESSAGE(ns(from, to) <= (max) + 0.01, name << ": " << ns(from, to) << " > " << (max))

void check_strobes(const Edges& e)
{
    AT_LEAST("tRAS", e.ras_fall, e.ras_rise, 4 * kT + 35);
    AT_LEAST("tCAS", e.cas_fall, e.cas_rise, 3 * kT - 90);
    AT_LEAST("tPIP", e.pi_rise, e.pi_fall, 2 * kT - 47);
    AT_LEAST("tRSC", e.ras_fall, e.cas_fall, kT + 10);
    AT_LEAST("tCSP", e.cas_fall, e.pi_rise, kT - 28);
    AT_LEAST("tRSP", e.ras_fall, e.pi_rise, 2 * kT + 10);
    AT_LEAST("tCSR", e.cas_fall, e.ras_rise, 3 * kT - 40);
    AT_LEAST("tPSC", e.pi_rise, e.cas_rise, 2 * kT - 75);
    AT_LEAST("tPSR", e.pi_rise, e.ras_rise, 2 * kT + 14);
    AT_LEAST("tRHC", e.cas_rise, e.ras_rise, 50);
    AT_LEAST("tRHP", e.pi_fall, e.ras_rise, 10);
    AT_LEAST("tPHC", e.cas_rise, e.pi_fall, 10);
    AT_LEAST("tDSR", e.addr, e.ras_fall, kT - 48);
    AT_LEAST("tDSC", e.addr, e.cas_fall, 2 * kT - 22);
    AT_LEAST("tDSP", e.addr, e.pi_rise, 3 * kT - 20);
    AT_LEAST("tCDE", e.cas_rise, e.next, kT - 18);
    AT_LEAST("tRDE", e.ras_rise, e.next, kT - 118);
    AT_LEAST("tOPW", e.cout_rise1, e.cout_fall1, kT - 33);
    AT_LEAST("tORT", e.cout_fall1, e.cout_rise2, 2 * kT - 37);
    CHECK_MESSAGE(e.cout_rise1 <= e.ras_fall, "tMOR");
    AT_LEAST("tMRO", e.ras_fall, e.cout_fall1, kT - 51);
    AT_LEAST("tOSC", e.cout_fall1, e.cas_fall, 10);
}

// The precharge of each strobe between two transactions in a row.
void check_precharge(const Edges& a, const Edges& b)
{
    AT_LEAST("tRPR", a.ras_rise, b.ras_fall, 2 * kT - 120);
    AT_LEAST("tCPR", a.cas_rise, b.cas_fall, 3 * kT - 5);
    AT_LEAST("tPPR", a.pi_fall, b.pi_rise, 4 * kT - 33);
}

}  // namespace

TEST_CASE("КР1807ВМ1 bus: a fetch, a read and a write in a row")
{
    Bench b({{READ, 0172000, 0, 3, true}, {READ, 01000}, {WRITE, 01002, 0xBEEF, 3},
             {WRITE, 01004, 0x5A5A, 1}, {READ, 01002}});
    REQUIRE(b.takes.size() == 6);
    for (std::size_t i = 0; i + 1 < b.takes.size(); ++i)
        CHECK(b.takes[i + 1] - b.takes[i] == 6 * kTicksPerT);
    CHECK(b.read_data == std::vector<std::uint16_t>{012700, 0x1234, 0xBEEF});
    CHECK(b.memory[01004] == 0x005A);
    CHECK(b.ai_seen == std::vector<int>{0xA5, 0xA5, 0xA5});

    for (std::size_t i = 0; i < 5; ++i) {
        CAPTURE(i);
        const Edges e = edges_of(b, i);
        check_strobes(e);
        if (i > 0)
            check_precharge(edges_of(b, i - 1), e);
    }
    // the fetch flag: SEL0 from the address to PI
    const Edges f = edges_of(b, 0);
    CHECK(b.s[static_cast<std::size_t>(f.addr)].sel0);
    AT_LEAST("tFFR", f.addr, f.ras_fall, kT - 23);
    AT_LEAST("tSSF", f.addr, edge(b, f.addr, &Sample::sel0, false), 3 * kT - 38);
    CHECK(!b.s[static_cast<std::size_t>(edges_of(b, 1).addr)].sel0);
}

TEST_CASE("КР1807ВМ1 bus: a read lets DAL go at -CAS and takes the data at its rise")
{
    Bench b({{READ, 01000}});
    const Edges e = edges_of(b, 0);
    for (int t = e.addr; t < e.cas_fall; ++t)
        CHECK(b.s[static_cast<std::size_t>(t)].dal_oe == 0xFFFF);
    for (int t = e.cas_fall; t < e.next; ++t)
        CHECK(b.s[static_cast<std::size_t>(t)].dal_oe == 0);
    AT_LEAST("tDHR", e.ras_fall, e.cas_fall, kT - 12);
    CHECK(b.read_data == std::vector<std::uint16_t>{0x1234});
}

TEST_CASE("КР1807ВМ1 bus: a write in normal mode")
{
    Bench b({{WRITE, 01000, 0xCAFE, 3}, {NOP}});
    const Edges e = edges_of(b, 0);
    const int rw_fall = edge(b, e.addr, &Sample::rwlb, false);
    const int rw_rise = edge(b, rw_fall, &Sample::rwlb, true);
    const int data_on = e.cas_fall;
    CHECK(b.s[static_cast<std::size_t>(data_on)].dal_out == 0xCAFE);
    CHECK(rw_fall == e.addr);
    AT_LEAST("tNSR", rw_fall, e.ras_fall, kT - 78);
    AT_LEAST("tNSC", rw_fall, e.cas_fall, 2 * kT - 37);
    AT_LEAST("tNSP", rw_fall, e.pi_rise, 3 * kT - 45);
    AT_LEAST("tNHR", e.ras_rise, rw_rise, kT - 108);
    AT_LEAST("tNHC", e.cas_rise, rw_rise, kT - 32);
    AT_LEAST("tNHP", e.pi_fall, rw_rise, kT - 43);
    AT_LEAST("tNMP", rw_fall, rw_rise, 6 * kT - 66);
    AT_LEAST("tDHN", rw_fall, data_on, 2 * kT - 20);
    AT_MOST("tCWD", e.cas_fall, data_on, 80);
    AT_MOST("tRWD", e.ras_fall, data_on, 2 * kT + 4);
    AT_LEAST("tWDP", data_on, e.pi_rise, kT - 83);
    AT_LEAST("tWSC", data_on, e.cas_rise, 3 * kT - 150);
    AT_LEAST("tWSP", data_on, e.pi_fall, 3 * kT - 110);
    AT_LEAST("tWSR", data_on, e.ras_rise, 3 * kT - 55);
    AT_LEAST("tWHC", e.cas_rise, e.next, kT - 28);
    AT_LEAST("tWHP", e.pi_fall, e.next, kT - 88);
    AT_LEAST("tWHR", e.ras_rise, e.next, kT - 118);
    CHECK(b.memory[01000] == 0xCAFE);
    CHECK(b.ai_seen.empty());                   // a write does not sample requests
}

TEST_CASE("КР1807ВМ1 bus: byte writes strobe one half")
{
    Bench b({{WRITE, 01000, 0x1111, 2}, {WRITE, 01000, 0x2222, 1}});
    CHECK(b.memory[01000] == 0x1122);
}

TEST_CASE("КР1807ВМ1 bus: delayed R/W strobes with -CAS")
{
    Bench b({{WRITE, 01000, 0xCAFE, 3}}, true);
    const Edges e = edges_of(b, 0);
    CHECK(edge(b, e.addr, &Sample::rwlb, false) == e.cas_fall);
    CHECK(edge(b, e.cas_fall, &Sample::rwlb, true) == e.cas_rise);
    CHECK(b.memory[01000] == 0xCAFE);
}

TEST_CASE("КР1807ВМ1 bus: a bus NOP is 3T with no strobes and one COUT pulse")
{
    Bench b({{NOP}, {NOP}, {READ, 01000}});
    CHECK(b.takes[1] - b.takes[0] == 3 * kTicksPerT);
    CHECK(b.takes[2] - b.takes[1] == 3 * kTicksPerT);
    for (int t = b.takes[0]; t < b.takes[2]; ++t) {
        const Sample& x = b.s[static_cast<std::size_t>(t)];
        CHECK((x.ras_n && x.cas_n && !x.pi && !x.sel0 && !x.sel1 && x.rwhb && x.rwlb));
    }
    const int rise = edge(b, b.takes[0], &Sample::cout, true);
    CHECK(rise - b.takes[0] == kTicksPerT);
    CHECK(edge(b, rise, &Sample::cout, false) - rise == kTicksPerT);
}

TEST_CASE("КР1807ВМ1 bus: ASPI pulses -CAS and PI without -RAS and samples the requests")
{
    Bench b({{ASPI}, {NOP}});
    const int t0 = b.takes[0];
    CHECK(b.takes[1] - t0 == 4 * kTicksPerT);
    const int cas_fall = edge(b, t0, &Sample::cas_n, false);
    const int pi_rise = edge(b, t0, &Sample::pi, true);
    CHECK(cas_fall == t0);
    AT_LEAST("tCSP", cas_fall, pi_rise, kT - 28);
    CHECK(edge(b, t0, &Sample::ras_n, false) < 0);
    CHECK(b.ai_seen == std::vector<int>{0xA5});
    CHECK(b.s[static_cast<std::size_t>(t0)].dal_oe == 0);
}

TEST_CASE("КР1807ВМ1 bus: IACK shows the request and reads the vector at SEL1 falling")
{
    Bench b({{IACK, 0, 0, 3, false, 0x0A}, {NOP}});
    const int t0 = b.takes[0];
    CHECK(b.takes[1] - t0 == 4 * kTicksPerT);
    const int sel1_fall = edge(b, t0, &Sample::sel1, false);
    const int ras_fall = edge(b, t0, &Sample::ras_n, false);
    const int ras_rise = edge(b, ras_fall, &Sample::ras_n, true);
    CHECK(b.s[static_cast<std::size_t>(t0)].sel1);
    CHECK(b.s[static_cast<std::size_t>(t0)].dal_out == 0x0A00);
    CHECK(b.s[static_cast<std::size_t>(t0)].dal_oe == 0xFF00);
    AT_LEAST("tKKR", t0, ras_fall, kT - 63);
    AT_LEAST("tDSR", t0, ras_fall, kT - 48);
    AT_LEAST("tKRP", ras_fall, ras_rise, 2 * kT + 35);
    AT_LEAST("tKSP", t0, sel1_fall, 3 * kT - 66);
    AT_LEAST("tRHS", sel1_fall, ras_rise, 45);
    CHECK(edge(b, t0, &Sample::cas_n, false) < 0);
    CHECK(edge(b, t0, &Sample::pi, true) < 0);
    CHECK(b.vectors == std::vector<std::uint16_t>{0x0134});
}

TEST_CASE("КР1807ВМ1 bus: constant COUT is the clock divided by two")
{
    Bench b({{NOP}, {NOP}}, false, false);
    const int r1 = edge(b, b.takes[0], &Sample::cout, true);
    const int f1 = edge(b, r1, &Sample::cout, false);
    const int r2 = edge(b, f1, &Sample::cout, true);
    CHECK(f1 - r1 == kTicksPerT);
    CHECK(r2 - r1 == 2 * kTicksPerT);
}
