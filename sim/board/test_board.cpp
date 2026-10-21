// The generated НС4 board (rtl/board/ns4.sv) on nothing but the master
// clock, its crystals inside: its clocks, its power-on and the processor
// running ROM-B.
#include <doctest/doctest.h>
#include <verilated.h>

#include "Vns4.h"
#include "power.h"

extern "C" {
#include <ms0515/core/board.h>
#include <ms0515/core/cpu.h>
}

#include <cstdint>
#include <fstream>
#include <iterator>
#include <memory>
#include <sstream>
#include <string>
#include <type_traits>
#include <vector>

namespace {

// The master clock is 120 MHz: the 8 MHz crystal is a period of 15 master
// ticks, the 15 MHz one of 8.
constexpr int kMasterMhz = 120;

// The state every register, latch and RAM cell powers up in: all zero
// (seed 0), or Verilator's random values from the seed - the state the
// datasheets leave undefined until a reset, which the board must come out
// of by itself.  Set before the model is made.
struct PowerUp {
    explicit PowerUp(int seed)
    {
        Verilated::randReset(seed == 0 ? 0 : 2);
        Verilated::randSeed(seed);
    }
};

class Board {
public:
    explicit Board(int seed = 0) : power_up_(seed), m_(std::make_unique<Vns4>()) { disconnect(*m_); }

    // One master tick: the rising edge.
    void tick()
    {
        power(*m_, t_++);
        m_->clk = 0;
        m_->eval();
        m_->clk = 1;
        m_->eval();
    }

    Vns4& m() { return *m_; }

private:
    PowerUp power_up_;
    std::unique_ptr<Vns4> m_;
    std::uint64_t t_ = 0;
};

// Rising edges of a net over `ticks` master ticks.
template <class Net>
int rises(Board& b, int ticks, Net net)
{
    int count = 0;
    bool was = net(b.m()) != 0;
    for (int i = 0; i < ticks; ++i) {
        b.tick();
        const bool now = net(b.m()) != 0;
        count += (now && !was) ? 1 : 0;
        was = now;
    }
    return count;
}

}  // namespace

TEST_CASE("the 8 MHz crystal comes out on ТС8М L through D15's gates 6 and 5")
{
    // two К555ЛН1 gates of about 9.5 ns each: two master ticks
    Board b;
    std::vector<int> crystal;                           // BQ1 pin 1, tick by tick
    for (int i = 0; i < 100; ++i) {
        b.tick();
        crystal.push_back(b.m().nx_C5_2);
        if (i >= 2)
            CHECK(b.m().n_TS8M_L == crystal[static_cast<std::size_t>(i - 2)]);
    }
    CHECK(rises(b, 100 * kMasterMhz, [](Vns4& m) { return m.nx_C5_2; }) ==
          doctest::Approx(800).epsilon(0.01));
}

TEST_CASE("D16 divides ТС8М into ТС4М, ТС2М and ТС1М")
{
    Board b;
    rises(b, 64, [](Vns4& m) { return m.n_TS8M_L; });     // let the counter run in
    const int us = 100 * kMasterMhz;                      // 100 microseconds
    CHECK(rises(b, us, [](Vns4& m) { return m.n_TS8M_L; }) == doctest::Approx(800).epsilon(0.01));
    CHECK(rises(b, us, [](Vns4& m) { return m.n_TS4M_H; }) == doctest::Approx(400).epsilon(0.01));
    CHECK(rises(b, us, [](Vns4& m) { return m.n_TS2M_H; }) == doctest::Approx(200).epsilon(0.01));
    CHECK(rises(b, us, [](Vns4& m) { return m.n_TS1M_H; }) == doctest::Approx(100).epsilon(0.01));
}

TEST_CASE("the 15 MHz crystal comes out on ТС15 H and ТС15 L")
{
    Board b;
    const int us = 100 * kMasterMhz;
    CHECK(rises(b, us, [](Vns4& m) { return m.n_TS15_H; }) == doctest::Approx(1500).epsilon(0.01));
    // ТС15 L is ТС15 H through D31's NAND, a tick later
    int was = b.m().n_TS15_H;
    for (int i = 0; i < 100; ++i) {
        b.tick();
        CHECK(b.m().n_TS15_L == !was);
        was = b.m().n_TS15_H;
    }
}

namespace {

// The ticks a net fell at.
struct Falls {
    std::vector<std::int64_t> at;
    bool was = true;
    void see(bool now, std::int64_t t)
    {
        if (!now && was) at.push_back(t);
        was = now;
    }
    double period_us(std::size_t from) const
    {
        return static_cast<double>(at.back() - at[from]) / static_cast<double>(at.size() - 1 - from) /
               kMasterMhz;
    }
};

// bit n of a Verilator vector: a number up to 64 bits, an array of words above
template <class T>
bool bit_of(const T& v, int n)
{
    if constexpr (std::is_integral_v<T>)
        return n < 64 && ((static_cast<std::uint64_t>(v) >> n) & 1u) != 0;
    else
        return static_cast<std::size_t>(n / 32) < sizeof(v) / 4 &&
               ((v[static_cast<std::size_t>(n / 32)] >> (n % 32)) & 1u) != 0;
}

// the word on МК00..МК15
std::uint16_t mk(const Vns4& m)
{
    const CData bits[16] = {m.n_MK00_H, m.n_MK01_H, m.n_MK02_H, m.n_MK03_H, m.n_MK04_H, m.n_MK05_H,
                            m.n_MK06_H, m.n_MK07_H, m.n_MK08_H, m.n_MK09_H, m.n_MK10_H, m.n_MK11_H,
                            m.n_MK12_H, m.n_MK13_H, m.n_MK14_H, m.n_MK15_H};
    std::uint16_t w = 0;
    for (int i = 0; i < 16; ++i)
        w = static_cast<std::uint16_t>(w | (bits[i] & 1u) << i);
    return w;
}

// The emulator from reset with ROM-B: the PC of each of its first n instructions.
std::vector<std::uint16_t> emulator_fetches(std::size_t n)
{
    std::ifstream f(MS0515_FIRMWARE_DIR "/ms0515-romb.rom", std::ios::binary);
    REQUIRE(f);
    const std::vector<char> rom{std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>()};
    auto board = std::make_unique<ms0515_board_t>();
    board_init(board.get());
    board_load_rom(board.get(), reinterpret_cast<const std::uint8_t*>(rom.data()),
                   static_cast<std::uint32_t>(rom.size()));
    board_reset(board.get());
    std::vector<std::uint16_t> pcs;
    while (pcs.size() < n) {
        pcs.push_back(board->cpu.r[7]);
        cpu_step(&board->cpu);
    }
    return pcs;
}

// Two drivers on one net.  As one lets a bus go and the next takes it, the
// first may still drive for its worst-case turn-off (an EPROM's tDF of
// 130 ns, from a datasheet that gives maximums only) while the next drives
// at its typical time: an overlap of a tick or two, as on a real board at
// its worst.  Up to kHandover ticks is such a handover, counted; longer is
// a fight - a driver on that should not be.
struct Fights {
    static constexpr int kHandover = 3;
    int handovers = 0;
    std::vector<int> run = std::vector<int>(32 * 4, 0);

    // the bits fighting longer than a handover, as text; empty if none
    template <class V>
    std::string see(const V& m)
    {
        std::string bad;
        for (int n = 0; n < static_cast<int>(run.size()); ++n) {
            auto& r = run[static_cast<std::size_t>(n)];
            if (bit_of(m.contention_at, n)) {
                if (++r > kHandover)
                    bad += std::to_string(n) + " ";
            } else {
                handovers += r > 0 ? 1 : 0;
                r = 0;
            }
        }
        return bad;
    }
};

std::string octal(unsigned v)
{
    std::ostringstream s;
    s << std::oct << v;
    return s.str();
}

}  // namespace

// The first 100 ms after power-on: the sync PROMs' timing, D16's reset pulse
// and the processor let go, from then on no net with two drivers on, and
// the processor running ROM-B as the emulator does.  The owner's jumpers
// (E4 fitted, E3 open: РВК0 low, РВК1 high) are the 60 Hz row of the ТО's
// Table 10.
static void power_on(int seed)
{
    INFO("power-up state seed " << seed);
    Board b(seed);
    Falls line, frame;
    Fights fights;
    std::int64_t start_rose = -1, start_fell = -1;
    std::vector<std::uint16_t> fetches;
    bool ras_was = true;
    const std::int64_t ticks = 100000 * static_cast<std::int64_t>(kMasterMhz);
    for (std::int64_t t = 0; t < ticks; ++t) {
        b.tick();
        Vns4& m = b.m();
        line.see(m.n_SSI_L != 0, t);
        frame.see(m.n_KSI_L != 0, t);
        // СТАРТ is the pulse once the supply is up (before, a random
        // power-up state may show anything there for a tick)
        if (start_rose < 0 && m.n_START_H && t >= static_cast<std::int64_t>(kSupplyRiseTicks)) start_rose = t;
        if (start_rose >= 0 && start_fell < 0 && !m.n_START_H) start_fell = t;
        if (start_fell >= 0 && !m.n_PVAS_L && ras_was && m.n_REZH0_H)
            fetches.push_back(mk(m));
        ras_was = m.n_PVAS_L != 0;
        if (start_fell >= 0) {
            const std::string bits = fights.see(m);
            if (!bits.empty())
                FAIL("contention at " << t / kMasterMhz << " us on bits " << bits << "(ns4.sv's header)");
        }
    }
    MESSAGE(fights.handovers << " bus handovers overlapped by at most " << Fights::kHandover << " ticks");
    REQUIRE(line.at.size() > 100);
    CHECK(line.period_us(1) == doctest::Approx(64.0).epsilon(0.001));
    REQUIRE(frame.at.size() >= 4);
    // The last frame: until СТАРТ resets D76 (КР580ВВ55), whose state is
    // undefined before a RESET, its port B may drive РВК1 low and give the
    // sync PROM D13 the 50 Hz page - the first frames' length is not fixed.
    CHECK(frame.period_us(frame.at.size() - 2) == doctest::Approx(260 * 64.0).epsilon(0.01));
    // D16 counts АПК7: the processor is held from its 2nd fall to its 4th
    REQUIRE(start_rose > 0);
    REQUIRE(start_fell > start_rose);
    CHECK(static_cast<double>(start_fell - start_rose) / kMasterMhz ==
          doctest::Approx(2 * 128 * 2 * 64.0).epsilon(0.02));

    REQUIRE(fetches.size() > 100);
    const std::vector<std::uint16_t> expected = emulator_fetches(fetches.size());
    std::size_t same = 0;
    while (same < fetches.size() && fetches[same] == expected[same])
        ++same;
    std::string around;
    for (std::size_t i = same >= 8 ? same - 8 : 0; i < same + 4 && i < fetches.size(); ++i)
        around += octal(fetches[i]) + (i < expected.size() ? "/" + octal(expected[i]) : "") + " ";
    MESSAGE(fetches.size() << " fetches, the first " << same << " as the emulator's; board/emulator around: "
                           << around);
    CHECK(same == fetches.size());
}

TEST_CASE("power-on: 64 us lines, 60 Hz frames, СТАРТ H, then ROM-B as in the emulator")
{
    power_on(0);
}

// The same from undefined power-up states: the board must reset itself.
TEST_CASE("power-on from any state: three random power-up states")
{
    for (const int seed : {1, 2, 3})
        power_on(seed);
}
