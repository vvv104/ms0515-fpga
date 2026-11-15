// Beam: the line as the ticks its colour changes at - a colour held three
// ticks is three ticks, not a point or nothing; a line from each fall of
// ССИ L, a frame to each fall of КСИ L.
#include <doctest/doctest.h>

#include "beam.h"

#include <sstream>
#include <string>
#include <vector>

namespace {

struct Lines {
    std::uint8_t n_KSI_L = 1, n_SSI_L = 1;
    std::uint8_t n_SC_H = 0, n_KC_H = 0, n_ZC_H = 0, nx_D69_9 = 0;
    void colour(int v)
    {
        n_SC_H = v & 1;
        n_KC_H = (v >> 1) & 1;
        n_ZC_H = (v >> 2) & 1;
        nx_D69_9 = (v >> 3) & 1;
    }
};

// lines of colours a tick each, ССИ L falling at each line's first tick;
// then the frame's end
Beam::Lines beam_of(const std::vector<std::vector<int>>& frame)
{
    Beam beam;
    Lines m;
    beam.see(m);
    for (const auto& line : frame) {
        for (std::size_t t = 0; t < line.size(); ++t) {
            m.n_SSI_L = t == 0 ? 0 : 1;
            m.colour(line[t]);
            beam.see(m);
        }
    }
    m.n_KSI_L = 0;
    beam.see(m);
    return beam.last;
}

bool same(const Beam::Line& line, const std::vector<std::pair<int, int>>& want)
{
    if (line.size() != want.size())
        return false;
    for (std::size_t i = 0; i < want.size(); ++i)
        if (line[i].tick != want[i].first || line[i].colour != want[i].second)
            return false;
    return true;
}

}  // namespace

TEST_CASE("a line is the ticks its colour changes at, a short colour kept")
{
    const auto frame = beam_of({{0, 0, 7, 7, 7, 13, 0, 0}, {5, 5, 5}});
    REQUIRE(frame.size() == 2);
    CHECK(same(frame[0], {{0, 0}, {2, 7}, {5, 13}, {6, 0}}));
    CHECK(same(frame[1], {{0, 5}}));
}

TEST_CASE("points are taken on the output's own grid, not the clock's")
{
    // points of 8 ticks changing at tick 4 mod 8, the picture opening 2
    // ticks into its first point (6 of 8 shown) and closing on a point
    std::vector<int> line(4, 0);
    line.insert(line.end(), 2, 0);
    line.insert(line.end(), 6, 7);      // the first point, cut to 6 ticks
    line.insert(line.end(), 8, 5);
    line.insert(line.end(), 8, 7);
    line.insert(line.end(), 8, 0);      // blank
    const auto frame = beam_of({line, line});
    const auto samples = Beam::samples(frame, 36);
    REQUIRE(samples.size() == 2);
    CHECK(samples[0] == std::vector<std::uint8_t>({0, 7, 5, 7, 0}));
}

TEST_CASE("a frame is written as its lines' changes")
{
    const auto frame = beam_of({{0, 9, 9}});
    std::ostringstream out;
    Beam::write(out, frame);
    const std::string want("\x01\x00" "\x02\x00" "\x00\x00\x00" "\x01\x00\x09", 10);
    CHECK(out.str() == want);
}
