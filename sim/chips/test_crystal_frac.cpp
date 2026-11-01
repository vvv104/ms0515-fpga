// crystal_frac: the МС 7004's 4.608 MHz crystal off the 120 MHz master
// clock - 24 cycles in every 625 ticks, each of 26 or 27 ticks.
#include <doctest/doctest.h>

#include "Vcrystal_frac.h"

#include <memory>
#include <vector>

TEST_CASE("crystal_frac: 4608 kHz - 24 cycles in 625 ticks, of 26 and 27")
{
    auto m = std::make_unique<Vcrystal_frac>();
    std::vector<int> rises;
    int was = 0;
    for (int t = 0; t < 625 * 8; ++t) {
        m->clk = 0;
        m->eval();
        m->clk = 1;
        m->eval();
        if (m->p1 && !was) rises.push_back(t);
        was = m->p1;
    }
    REQUIRE(rises.size() >= 2);
    for (std::size_t i = 1; i < rises.size(); ++i) {
        const int period = rises[i] - rises[i - 1];
        CHECK((period == 26 || period == 27));
    }
    // over 625 ticks from any rise: 24 cycles
    for (std::size_t i = 0; i + 24 < rises.size(); ++i)
        CHECK(rises[i + 24] - rises[i] == 625);
}
