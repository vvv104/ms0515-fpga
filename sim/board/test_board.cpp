// The generated НС4 board (rtl/board/ns4.sv) with nothing but its two
// crystals driven: the clock nets the board derives from them.
#include <doctest/doctest.h>
#include <verilated.h>

#include "Vns4.h"

#include <cstdint>

namespace {

// The master clock is 120 MHz: the 8 MHz crystal is a period of 15 master
// ticks, the 15 MHz one of 8.
constexpr int kMasterMhz = 120;

class Board {
public:
    Board()
    {
        m_.x_BQ1_p1_oe = 1;
        m_.x_BQ2_p1_oe = 1;
    }

    // One master tick: the crystals' levels for it, then the rising edge.
    void tick()
    {
        m_.x_BQ1_p1 = (t_ % 15) < 8 ? 1 : 0;
        m_.x_BQ2_p1 = (t_ % 8) < 4 ? 1 : 0;
        m_.clk = 0;
        m_.eval();
        m_.clk = 1;
        m_.eval();
        ++t_;
    }

    Vns4& m() { return m_; }

private:
    Vns4 m_;
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
    Board b;
    for (int i = 0; i < 100; ++i) {
        b.tick();
        CHECK(b.m().n_TS8M_L == b.m().x_BQ1_p1);
    }
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
    for (int i = 0; i < 100; ++i) {
        b.tick();
        CHECK(b.m().n_TS15_L != b.m().n_TS15_H);
    }
}

TEST_CASE("no two drivers fight over a net while the clocks run")
{
    Board b;
    for (int i = 0; i < 10000; ++i) {
        b.tick();
        REQUIRE(b.m().contention == 0);
    }
}
