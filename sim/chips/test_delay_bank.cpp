// delay_bank: W chips' outputs through one propagation delay, inertial as
// a gate's is - each bit takes a level once its input has held it for the
// whole delay (TICKS master ticks to rise, FALL to fall), so a pulse
// shorter than the delay never comes out.  One bit, 3 and 3; one bit, 3
// and 1 (an output enable's tPZH and shorter tPHZ); three bits at once.
#include <doctest/doctest.h>

#include "Vdelay_bank.h"
#include "Vdelay_bank_asym.h"
#include "Vdelay_bank_wide.h"
#include "Vdelay_bank_long.h"
#include "Vdelay_bank_one.h"

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

TEST_CASE("delay_bank: a level held long enough comes out three ticks later")
{
    // the input set before tick t shows after tick t + 2 (a tick less than
    // TICKS: the first is the tick the input is taken at)
    const std::vector<int> in = {0, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 1, 1, 1, 1};
    const std::vector<int> want = {0, 0, 0, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 1, 1};
    CHECK(run<Vdelay_bank>(in) == want);
}

TEST_CASE("delay_bank: a pulse shorter than the delay never comes out")
{
    const std::vector<int> in = {0, 0, 1, 0, 0, 1, 1, 0, 0, 0, 0, 0};
    const std::vector<int> want = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    CHECK(run<Vdelay_bank>(in) == want);
}

TEST_CASE("delay_bank: a rise and a fall delayed apart")
{
    const std::vector<int> in = {0, 0, 1, 1, 1, 1, 1, 0, 0, 0, 1, 1, 1, 1, 0, 0};
    // a rise shows two ticks after the tick it came in, a fall at once
    const std::vector<int> want = {0, 0, 0, 0, 1, 1, 1, 0, 0, 0, 0, 0, 1, 1, 0, 0};
    CHECK(run<Vdelay_bank_asym>(in) == want);
}

TEST_CASE("delay_bank: three bits, each as alone")
{
    // bit 0 the first case's input, bit 1 the second's, bit 2 always high
    const std::vector<int> a = {0, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 1};
    const std::vector<int> b = {0, 0, 1, 0, 0, 1, 1, 0, 0, 0, 0, 0};
    std::vector<int> in;
    for (std::size_t t = 0; t < a.size(); ++t)
        in.push_back(a[t] | b[t] << 1 | 4);
    const std::vector<int> want_a = {0, 0, 0, 1, 1, 1, 1, 1, 0, 0, 0, 0};
    const std::vector<int> got = run<Vdelay_bank_wide>(in);
    for (std::size_t t = 0; t < a.size(); ++t) {
        CHECK((got[t] & 1) == want_a[t]);
        CHECK((got[t] >> 1 & 1) == 0);
        CHECK((got[t] >> 2 & 1) == (t >= 2 ? 1 : 0));
    }
}

namespace {

// the rule itself: an output takes a level once the input has held it for
// the whole delay, counting the tick the input is taken at
std::vector<int> rule(const std::vector<int>& in, int ticks, int fall)
{
    std::vector<int> out;
    int level = 0, high = 0, low = 0;
    for (const int v : in) {
        high = v ? high + 1 : 0;
        low = v ? 0 : low + 1;
        if (high >= ticks) level = 1;
        if (low >= fall) level = 0;
        out.push_back(level);
    }
    return out;
}

// runs of every length, short ones often
std::vector<int> noise(std::size_t n, unsigned seed)
{
    std::vector<int> in;
    unsigned x = seed;
    int v = 0;
    for (std::size_t t = 0; t < n; ++t) {
        x = x * 1103515245u + 12345u;
        if ((x >> 16) % 7 == 0) v = !v;
        in.push_back(v);
    }
    return in;
}

}  // namespace

TEST_CASE("delay_bank: long and short delays follow the rule on any input")
{
    for (unsigned seed = 1; seed <= 4; ++seed) {
        const std::vector<int> in = noise(3000, seed);
        CHECK(run<Vdelay_bank_long>(in) == rule(in, 18, 16));
        CHECK(run<Vdelay_bank_one>(in) == rule(in, 1, 1));
        CHECK(run<Vdelay_bank>(in) == rule(in, 3, 3));
        CHECK(run<Vdelay_bank_asym>(in) == rule(in, 3, 1));
    }
}
