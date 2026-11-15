// FrameGrab: a point's colour is the one its 8 master ticks held longest,
// as a monitor shows it - a change a few ticks into the point counts, a
// spike shorter than half a point does not.
#include <doctest/doctest.h>

#include "frame_grab.h"

#include <cstdint>
#include <vector>

namespace {

// the nets the grabber reads, as a board model has them
struct Lines {
    std::uint8_t n_KSI_L = 1, n_SSI_L = 1, n_TS15_H = 0;
    std::uint8_t n_SC_H = 0, n_KC_H = 0, n_ZC_H = 0, nx_D69_9 = 0;
    void colour(int v)
    {
        n_SC_H = v & 1;
        n_KC_H = (v >> 1) & 1;
        n_ZC_H = (v >> 2) & 1;
        nx_D69_9 = (v >> 3) & 1;
    }
};

// one line of points, each 8 ticks from a rise of ТС15 H; colours[k][t] the
// colour at tick t of point k; then the frame's end
FrameGrab::Lines grab_line(const std::vector<std::vector<int>>& colours)
{
    FrameGrab grab;
    Lines m;
    grab.see(m);
    m.n_SSI_L = 0;                      // the line begins
    grab.see(m);
    for (const auto& point : colours)
        for (int t = 0; t < 8; ++t) {
            m.n_TS15_H = t < 4 ? 1 : 0;
            m.colour(point[static_cast<std::size_t>(t)]);
            grab.see(m);
        }
    m.n_TS15_H = 1;                     // the last point closed by the next rise
    grab.see(m);
    m.n_KSI_L = 0;
    grab.see(m);
    return grab.last;
}

}  // namespace

TEST_CASE("a point's colour is the one it held longest")
{
    // the colour changes two ticks into each point, as the board's does
    const auto lines = grab_line({{9, 9, 3, 3, 3, 3, 3, 3}, {3, 3, 5, 5, 5, 5, 5, 5}, {5, 5, 5, 5, 5, 5, 5, 5}});
    REQUIRE(lines.size() == 1);
    CHECK(lines[0] == std::vector<std::uint8_t>({3, 5, 5}));
}

TEST_CASE("a spike shorter than half a point is not a point")
{
    // the attributes of the next word three ticks early, before its points
    const auto lines = grab_line({{3, 3, 3, 3, 3, 6, 6, 6}, {6, 6, 6, 6, 6, 6, 6, 6}});
    REQUIRE(lines.size() == 1);
    CHECK(lines[0] == std::vector<std::uint8_t>({3, 6}));
}
