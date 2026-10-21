// delay_line: a chip's propagation delay, inertial as a gate's is - the
// output takes a level once the input has held it for the whole delay
// (TICKS master ticks to rise, FALL to fall), so a pulse shorter than the
// delay never comes out.  3 and 3 here, and 3 and 1 for an output enable's
// tPZH and its shorter tPHZ.
#include <doctest/doctest.h>

#include "Vdelay_line.h"
#include "Vdelay_line_asym.h"

#include <memory>
#include <vector>

namespace {

template <typename V>
std::vector<int> run(const std::vector<int>& in)
{
    auto m = std::make_unique<V>();
    std::vector<int> out;
    for (const int v : in) {
        m->i = static_cast<CData>(v);
        m->clk = 0;
        m->eval();
        m->clk = 1;
        m->eval();
        out.push_back(m->o);
    }
    return out;
}

}  // namespace

TEST_CASE("delay_line: a level held long enough comes out three ticks later")
{
    // the input set before tick t shows after tick t + 2 (a tick less than
    // TICKS: the first is the tick the input is taken at)
    const std::vector<int> in = {0, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 1, 1, 1, 1};
    const std::vector<int> want = {0, 0, 0, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 1, 1};
    CHECK(run<Vdelay_line>(in) == want);
}

TEST_CASE("delay_line: a pulse shorter than the delay never comes out")
{
    const std::vector<int> in = {0, 0, 1, 0, 0, 1, 1, 0, 0, 0, 0, 0};
    const std::vector<int> want = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    CHECK(run<Vdelay_line>(in) == want);
}

TEST_CASE("delay_line: a rise and a fall delayed apart")
{
    const std::vector<int> in = {0, 0, 1, 1, 1, 1, 1, 0, 0, 0, 1, 1, 1, 1, 0, 0};
    // a rise shows two ticks after the tick it came in, a fall at once
    const std::vector<int> want = {0, 0, 0, 0, 1, 1, 1, 0, 0, 0, 0, 0, 1, 1, 0, 0};
    CHECK(run<Vdelay_line_asym>(in) == want);
}
