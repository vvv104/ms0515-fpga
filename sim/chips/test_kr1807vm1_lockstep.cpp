// КР1807ВМ1 against the emulator's processor, instruction by instruction:
// random programs of the arithmetic, logic and branch instructions in
// every addressing mode run on both, the registers and the PS compared
// after each instruction and the memory at the end.
#include <doctest/doctest.h>

#include "Vkr1807vm1_public.h"
#include "Vkr1807vm1_public___024root.h"
#include "cpu_bench.h"

extern "C" {
#include <ms0515/core/board.h>
#include <ms0515/core/cpu.h>
}

#include <algorithm>
#include <cstdint>
#include <memory>
#include <random>
#include <sstream>
#include <string>
#include <vector>

namespace {

using Bench = CpuBench<Vkr1807vm1_public>;

constexpr std::uint16_t kProgram = 001000;     // the program, up to 03776
constexpr std::uint16_t kData = 004000;        // the data, 004000-007776
constexpr std::uint16_t kDataEnd = 010000;
constexpr std::uint16_t kStack = 000776;

std::string octal(unsigned v)
{
    std::ostringstream s;
    s << std::oct << v;
    return s.str();
}

// A random program: the registers set, then instructions whose operands
// are R0..R5 in all eight modes, the immediate, absolute and relative ones.
class Generator {
public:
    explicit Generator(unsigned seed) : rng_(seed) {}

    std::vector<std::uint16_t> program(int length)
    {
        std::vector<std::uint16_t> p;
        for (int r = 0; r < 6; ++r) {
            p.push_back(static_cast<std::uint16_t>(012700 | r));    // MOV #x, Rr
            p.push_back(r < 4 ? data_address() : random_word());
        }
        p.push_back(012706);                                        // MOV #stack, SP
        p.push_back(kStack);
        for (int i = 0; i < length; ++i)
            instruction(p);
        p.push_back(000777);                                        // BR . (the end)
        return p;
    }

private:
    std::uint16_t random_word() { return static_cast<std::uint16_t>(rng_()); }
    std::uint16_t data_address()
    {
        return static_cast<std::uint16_t>(kData + 2 * (rng_() % ((kDataEnd - kData) / 2)));
    }
    unsigned pick(unsigned n) { return static_cast<unsigned>(rng_() % n); }

    // An operand: its six-bit field, and the word after the instruction it
    // takes - an immediate, an absolute address, an index, or a relative
    // offset to be worked out from where the word lands.
    struct Operand {
        std::uint16_t field;
        bool has_word = false;
        std::uint16_t word = 0;
        bool relative = false;
    };

    Operand operand()
    {
        if (pick(6) == 0) {                                         // through PC
            static const unsigned pc_modes[] = {2, 3, 6, 7};
            const unsigned m = pc_modes[pick(4)];
            Operand o{static_cast<std::uint16_t>(m << 3 | 7), true, 0, m >= 6};
            o.word = m == 2 ? random_word() : data_address();       // relative: the target
            return o;
        }
        const unsigned mode = pick(8);
        Operand o{static_cast<std::uint16_t>(mode << 3 | pick(6))};
        if (mode >= 6) {
            o.has_word = true;
            o.word = static_cast<std::uint16_t>(2 * pick(8));
        }
        return o;
    }

    // Rn as the source and (Rn)+, -(Rn), @(Rn)+ or @-(Rn) as the
    // destination: the T-11 moves Rn before it reads it as the source (UG
    // §B.2), the emulator after - left to test_kr1807vm1's own case
    static bool quirk(std::uint16_t s, std::uint16_t d)
    {
        const unsigned dm = d >> 3 & 7;
        return (s >> 3) == 0 && (s & 7) == (d & 7) && dm >= 2 && dm <= 5;
    }

    // the instruction word, then the operands' words in order
    static void emit(std::vector<std::uint16_t>& p, std::uint16_t word,
                     const std::vector<Operand>& operands)
    {
        p.push_back(word);
        for (const Operand& o : operands) {
            if (!o.has_word) continue;
            const auto next = static_cast<std::uint16_t>(kProgram + 2 * (p.size() + 1));
            p.push_back(o.relative ? static_cast<std::uint16_t>(o.word - next) : o.word);
        }
    }

    void instruction(std::vector<std::uint16_t>& p)
    {
        static const std::uint16_t dops[] = {010000, 020000, 030000, 040000, 050000,
                                             060000, 0110000, 0120000, 0130000,
                                             0140000, 0150000, 0160000};
        static const std::uint16_t sops[] = {005000, 005100, 005200, 005300, 005400,
                                             005500, 005600, 005700, 006000, 006100,
                                             006200, 006300, 000300, 006700, 0105000,
                                             0105100, 0105200, 0105300, 0105400, 0105500,
                                             0105600, 0105700, 0106000, 0106100, 0106200,
                                             0106300, 0106700};
        switch (pick(10)) {
        case 0:
        case 1:
        case 2:
        case 3: {
            const Operand s = operand();
            Operand d = operand();
            while (quirk(s.field, d.field)) d = operand();
            emit(p, static_cast<std::uint16_t>(dops[pick(12)] | s.field << 6 | d.field), {s, d});
            return;
        }
        case 4:
        case 5:
        case 6: {
            const Operand d = operand();
            emit(p, static_cast<std::uint16_t>(sops[pick(27)] | d.field), {d});
            return;
        }
        case 7: {                                                   // XOR Rn, dst
            const unsigned r = pick(6);
            Operand d = operand();
            while (quirk(static_cast<std::uint16_t>(r), d.field)) d = operand();
            emit(p, static_cast<std::uint16_t>(074000 | r << 6 | d.field), {d});
            return;
        }
        case 8:                                                     // CCC / SCC
            p.push_back(static_cast<std::uint16_t>(000240 | pick(32)));
            return;
        default: {
            // a branch over the next, one-word instruction, or not
            static const std::uint16_t branches[] = {000400, 001000, 001400, 002000, 002400,
                                                     003000, 003400, 0100000, 0100400,
                                                     0101000, 0101400, 0102000, 0102400,
                                                     0103000, 0103400};
            p.push_back(static_cast<std::uint16_t>(branches[pick(15)] | 1));
            p.push_back(static_cast<std::uint16_t>(005204 | pick(2)));                  // INC R4/R5
            return;
        }
        }
    }

    std::mt19937 rng_;
};

// The model's state at the start of an instruction (its fetch).
struct State {
    std::uint16_t r[8];
    std::uint8_t ps;
    std::string bus;            // the model's transactions in the instruction before
    std::vector<std::pair<std::uint16_t, std::uint16_t>> writes;   // its words written
};

bool same(const State& a, const State& b)
{
    for (int i = 0; i < 8; ++i)
        if (a.r[i] != b.r[i]) return false;
    return a.ps == b.ps;
}

std::string show(const State& s)
{
    std::string t;
    for (int i = 0; i < 8; ++i)
        t += "R" + std::to_string(i) + "=" + octal(s.r[i]) + " ";
    return t + "PS=" + octal(s.ps);
}

State model_state(Bench& b)
{
    auto* root = b.m().rootp;
    State s{};
    for (int i = 0; i < 8; ++i)
        s.r[i] = root->kr1807vm1__DOT__core__DOT__r[static_cast<std::size_t>(i)];
    s.ps = root->kr1807vm1__DOT__core__DOT__ps;
    return s;
}

State emulator_state(const ms0515_cpu_t& c)
{
    State s{};
    for (int i = 0; i < 8; ++i)
        s.r[i] = c.r[i];
    s.ps = static_cast<std::uint8_t>(c.psw & 0377);
    return s;
}

// The model's run: the states at each fetch from the program's start on,
// or empty when it went outside the RAM the emulator has (a rejected
// program).
std::vector<State> run_model(Bench& b, const std::vector<std::uint16_t>& program,
                             std::size_t& instructions)
{
    b.load(0172000, {000137, kProgram});                    // JMP @#1000
    b.load(kProgram, program);
    b.power_up();
    std::vector<State> states;
    std::size_t seen = 0;
    const std::uint16_t end = static_cast<std::uint16_t>(kProgram + 2 * (program.size() - 1));
    while (true) {
        b.run_fetches(1, 20000);
        const auto& f = b.fetches();
        if (f.empty() || f.size() > 2000) return {};
        const std::uint16_t at = f.back().address;
        if (states.empty() && at != kProgram) continue;         // the JMP
        for (const auto& a : b.log)
            if (a.address >= 0160000 && a.address != 0172000 && a.address != 0172002 &&
                a.kind != Bench::Kind::Aspi)
                return {};
        states.push_back(model_state(b));
        std::string bus;
        for (std::size_t k = seen; k + 1 < b.log.size(); ++k) {
            static const char* const kinds[] = {"F", "R", "W", "A", "I"};
            const auto& a = b.log[k];
            bus += std::string(kinds[static_cast<int>(a.kind)]) + octal(a.address) +
                   (a.kind == Bench::Kind::Write ? "=" + octal(a.data) : "") + " ";
            if (a.kind == Bench::Kind::Write)
                states.back().writes.emplace_back(a.address, a.data);
        }
        seen = b.log.size();
        states.back().bus = bus;
        if (at == end) break;
    }
    instructions = states.size() - 1;
    return states;
}

}  // namespace

// The core acts on the bus's events - a transaction taken, read data, the
// AI pins, a vector - and they come at least kEventGap master ticks apart:
// what lets the FPGA give the core's arithmetic more than a tick.
TEST_CASE("КР1807ВМ1: the bus's events reach the core ticks apart")
{
    constexpr std::uint64_t kEventGap = 3;
    std::uint64_t closest = ~0ull, events = 0;
    for (unsigned seed = 1; seed <= 40; ++seed) {
        Generator g(seed);
        const std::vector<std::uint16_t> program = g.program(40);
        Bench b;
        b.load(0172000, {000137, kProgram});
        b.load(kProgram, program);
        b.power_up();
        std::uint64_t last = 0;
        bool seen = false;
        for (int t = 0; t < 40000; ++t) {
            b.run_ticks(1);
            auto* root = b.m().rootp;
            const int n = root->kr1807vm1__DOT__take + root->kr1807vm1__DOT__data_valid +
                          root->kr1807vm1__DOT__ai_valid + root->kr1807vm1__DOT__vector_valid;
            if (n == 0) continue;
            ++events;
            CHECK(n == 1);                                  // never two at once
            if (seen) closest = std::min(closest, b.now() - last);
            last = b.now();
            seen = true;
        }
    }
    MESSAGE(events << " events, the closest " << closest << " ticks apart");
    CHECK(events > 10000);
    CHECK(closest >= kEventGap);
}

TEST_CASE("КР1807ВМ1 and the emulator's processor: random programs, state after each instruction")
{
    int compared = 0;
    for (unsigned seed = 1; seed <= 400 && compared < 150; ++seed) {
        Generator g(seed);
        const std::vector<std::uint16_t> program = g.program(40);
        Bench b;
        for (std::uint16_t a = kData; a < kDataEnd; a = static_cast<std::uint16_t>(a + 2))
            b.word(a) = static_cast<std::uint16_t>(kData + ((a * 7u) % (kDataEnd - kData)) / 2 * 2);
        std::vector<std::uint16_t> data;
        for (std::uint16_t a = kData; a < kDataEnd; a = static_cast<std::uint16_t>(a + 2))
            data.push_back(b.word(a));
        std::size_t n = 0;
        const std::vector<State> states = run_model(b, program, n);
        if (states.empty()) continue;
        ++compared;

        auto board = std::make_unique<ms0515_board_t>();
        board_init(board.get());
        board_reset(board.get());
        for (std::size_t i = 0; i < program.size(); ++i)
            board_write_word(board.get(), static_cast<std::uint16_t>(kProgram + 2 * i), program[i]);
        for (std::size_t i = 0; i < data.size(); ++i)
            board_write_word(board.get(), static_cast<std::uint16_t>(kData + 2 * i), data[i]);
        ms0515_cpu_t& c = board->cpu;
        for (int i = 0; i < 8; ++i)
            c.r[i] = states[0].r[i];
        c.psw = states[0].ps;
        CAPTURE(seed);
        for (std::size_t i = 1; i < states.size(); ++i) {
            const std::uint16_t pc = c.r[7];
            const std::uint16_t op = board_read_word(board.get(), pc);
            cpu_step(&c);
            const State e = emulator_state(c);
            bool memory_same = true;
            for (const auto& [address, value] : states[i].writes)
                memory_same = memory_same &&
                              board_read_word(board.get(), static_cast<std::uint16_t>(address & ~1u)) == value;
            if (!same(states[i], e) || !memory_same) {
                FAIL_CHECK("after " << octal(op) << " at " << octal(pc) << "\n  model:    "
                                    << show(states[i]) << "\n  emulator: " << show(e)
                                    << "\n  before:   " << show(states[i - 1])
                                    << "\n  bus:      " << states[i].bus);
                break;
            }
        }
        for (std::uint16_t a = kData; a < kDataEnd; a = static_cast<std::uint16_t>(a + 2))
            if (b.word(a) != board_read_word(board.get(), a)) {
                FAIL_CHECK("memory at " << octal(a) << ": model " << octal(b.word(a))
                                        << ", emulator " << octal(board_read_word(board.get(), a)));
                break;
            }
    }
    MESSAGE(compared << " programs compared");
    CHECK(compared >= 100);
}
