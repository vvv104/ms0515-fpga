// A resistor pack read as logic (any_low): the common low when any line is.
// The board's one, the МС 7007's detector DA2, sums eleven lines.
#include <doctest/doctest.h>

#include "Vany_low.h"

#include <memory>

TEST_CASE("any_low: eleven lines, the common low when any is low")
{
    auto m = std::make_unique<Vany_low>();
    const unsigned all = (1u << 11) - 1;
    m->i = all;
    m->eval();
    CHECK(m->o == 1);
    for (int line = 0; line < 11; ++line) {
        m->i = all & ~(1u << line);
        m->eval();
        CHECK_MESSAGE(m->o == 0, "line ", line, " low");
    }
    m->i = 0;
    m->eval();
    CHECK(m->o == 0);
}
