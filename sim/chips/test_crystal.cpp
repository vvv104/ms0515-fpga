// A crystal oscillator: a square wave of PERIOD master ticks on pin 1, high
// for the first HIGH of them, from the first tick on.  Both of the board's:
// BQ1 8 MHz (15 ticks, 8 high) and BQ2 15 MHz (8 ticks, 4 high).
#include <doctest/doctest.h>

#include "Vcrystal.h"
#include "Vcrystal_15.h"

#include <memory>

namespace {

template <typename V>
void check_wave(int period, int high)
{
    auto m = std::make_unique<V>();
    for (int t = 0; t < 4 * period; ++t) {
        m->clk = 0;
        m->eval();
        CHECK_MESSAGE(m->p1 == ((t % period) < high ? 1 : 0), "tick ", t);
        m->clk = 1;
        m->eval();
    }
}

}  // namespace

TEST_CASE("crystal: 8 MHz, 15 ticks, 8 high")
{
    check_wave<Vcrystal>(15, 8);
}

TEST_CASE("crystal: 15 MHz, 8 ticks, 4 high")
{
    check_wave<Vcrystal_15>(8, 4);
}
