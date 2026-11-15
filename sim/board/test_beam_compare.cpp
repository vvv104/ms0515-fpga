// beam_compare on made-up beams: a frame as the ticks its colour changes
// at against the picture, each tick held to the picture within the
// tolerance in time - an edge a few nanoseconds off passes, a point
// shifted by a point, cut short or lost does not.
#include <doctest/doctest.h>

#include "beam_compare.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <vector>

namespace {

constexpr int kBorder = 5;
constexpr int kTop = 35;
constexpr int kX0 = 137 * 8 + 3;        // the 640 points' first tick, off the clock's grid
constexpr int kLineTicks = 6400;

using Vram = std::array<std::uint8_t, 16384>;

// 640 x 200: every word 1111000011001010, so runs of 4, 2 and 1 points
Vram stripes()
{
    Vram v{};
    for (std::size_t i = 0; i < v.size(); i += 2) {
        v[i] = 0xF0;
        v[i + 1] = 0xCA;
    }
    return v;
}

// 320 x 200: points 10110010, a word's last point dark and its first lit;
// the words' attributes in turn ink 2 on paper 6 and ink 4 on paper 3, word
// 3 of each line 0x8A flashing (ink 2, paper 1)
Vram attributes()
{
    Vram v{};
    for (std::size_t i = 0; i < v.size(); i += 2) {
        const std::size_t w = (i / 2) % 40;
        v[i] = 0xB2;
        v[i + 1] = w == 3 ? 0x8A : w % 2 ? 0x1C : 0x32;
    }
    return v;
}

// a line's colour at each tick as the beam keeps it: the ticks it changes at
Beam::Line line_of(const std::vector<std::uint8_t>& ticks)
{
    Beam::Line line;
    for (std::size_t t = 0; t < ticks.size(); ++t)
        if (t == 0 || ticks[t] != ticks[t - 1])
            line.push_back({static_cast<std::uint16_t>(t), ticks[t]});
    return line;
}

// The beam a board would draw of `v`: 262 lines of 6400 ticks, the border
// round the picture, a point 8 ticks from kX0; `shape` may change the
// line's ticks (y the picture's line, -1 for the border's) before it is kept.
Beam::Lines beam_of(const Vram& v, bool hires,
                    const std::function<void(int, std::vector<std::uint8_t>&)>& shape = nullptr)
{
    Beam::Lines frame;
    for (int row = 0; row < 262; ++row) {
        std::vector<std::uint8_t> ticks(kLineTicks, static_cast<std::uint8_t>(kBorder | 8));
        const int y = row - kTop;
        for (int x = 0; x < 640 && y >= 0 && y < 200; ++x) {
            int a = 0, b = 0;
            screen_detail::expect(v.data(), hires, kBorder, x, x, y, a, b);
            for (int k = 0; k < 8; ++k)
                ticks[static_cast<std::size_t>(kX0 + 8 * x + k)] = static_cast<std::uint8_t>(a);
        }
        if (shape)
            shape(y >= 0 && y < 200 ? y : -1, ticks);
        frame.push_back(line_of(ticks));
    }
    return frame;
}

// within `points` ticks for the points, `attributes` for the attributes
// (-1: as the points)
BeamCompare compare(const Beam::Lines& frame, const Vram& v, bool hires, int points, int attributes = -1,
                    bool held = false)
{
    return compare_beam(frame, v.data(), hires, kBorder, 137, 16, points, attributes < 0 ? points : attributes,
                        held);
}

}  // namespace

TEST_CASE("beam_compare: the picture itself differs nowhere and is found to the tick")
{
    const Vram v = stripes();
    const BeamCompare c = compare(beam_of(v, true), v, true, 0);
    CHECK(c.differ == 0);
    CHECK(c.x0 == kX0);
    CHECK(c.dy == kTop);
}

TEST_CASE("beam_compare: lit points rising 3 ticks late pass within 3 ticks, not within 2")
{
    const Vram v = stripes();
    // every rise to the lit colour comes 3 ticks after the picture's edge:
    // a lit point narrowed by 25 ns, as when two outputs' paths differ
    const Beam::Lines frame = beam_of(v, true, [](int y, std::vector<std::uint8_t>& ticks) {
        if (y < 0) return;
        const std::vector<std::uint8_t> was = ticks;
        const std::uint8_t lit = static_cast<std::uint8_t>((~kBorder & 7) | 8);
        for (std::size_t t = 3; t < ticks.size(); ++t)
            if (was[t] == lit && (was[t - 1] != lit || was[t - 2] != lit || was[t - 3] != lit))
                ticks[t] = static_cast<std::uint8_t>(kBorder | 8);
    });
    CHECK(compare(frame, v, true, 3).differ == 0);
    CHECK(compare(frame, v, true, 2).differ > 0);
}

TEST_CASE("beam_compare: a line's first word a point late is no edge's few ticks")
{
    const Vram v = stripes();
    // line 50's first 16 points come 8 ticks late, the first tick border
    const Beam::Lines frame = beam_of(v, true, [](int y, std::vector<std::uint8_t>& ticks) {
        if (y != 50) return;
        const std::vector<std::uint8_t> was = ticks;
        for (int t = kX0; t < kX0 + 8 * 16; ++t)
            ticks[static_cast<std::size_t>(t)] = was[static_cast<std::size_t>(t - 8)];
    });
    const BeamCompare c = compare(frame, v, true, 3);
    CHECK(c.differ > 0);
    CHECK(c.first_y == 50);
    CHECK(c.first_x < 16);
}

TEST_CASE("beam_compare: a point lost for 4 ticks inside a run is seen")
{
    const Vram v = stripes();
    // line 120, point 1 of word 7 - inside the run of 4 lit points - shows
    // paper for 4 of its 8 ticks
    const Beam::Lines frame = beam_of(v, true, [](int y, std::vector<std::uint8_t>& ticks) {
        if (y != 120) return;
        for (int k = 2; k < 6; ++k)
            ticks[static_cast<std::size_t>(kX0 + 8 * (7 * 16 + 1) + k)] = static_cast<std::uint8_t>(kBorder | 8);
    });
    const BeamCompare c = compare(frame, v, true, 3);
    CHECK(c.differ == 4);
    CHECK(c.first_x == 7 * 16 + 1);
    CHECK(c.first_y == 120);
}

TEST_CASE("beam_compare: no border, the line's blank meeting the picture 2 ticks into its first point")
{
    // at 72 Hz the line is blank (black) up to the picture and after it,
    // the blank ending a little after the points' grid
    const Vram v = stripes();
    const Beam::Lines frame = beam_of(v, true, [](int, std::vector<std::uint8_t>& ticks) {
        for (int t = 0; t < kX0 + 2; ++t)
            ticks[static_cast<std::size_t>(t)] = 0;
        for (int t = kX0 + 8 * 640 + 1; t < kLineTicks; ++t)
            ticks[static_cast<std::size_t>(t)] = 0;
    });
    CHECK(compare(frame, v, true, 3).differ == 0);
    CHECK(compare(frame, v, true, 1).differ > 0);
}

TEST_CASE("beam_compare: each of the DAC's inputs on its own - one opening a tick after another")
{
    // the blank ends 2 ticks into the first point; the red input comes on
    // there, the brightness a tick later: for a tick the point is red alone
    // (2), a mix of the blank and the point (10) no point has
    const Vram v = stripes();
    const Beam::Lines frame = beam_of(v, true, [](int y, std::vector<std::uint8_t>& ticks) {
        for (int t = 0; t < kX0 + 2; ++t)
            ticks[static_cast<std::size_t>(t)] = 0;
        if (y >= 0)
            ticks[static_cast<std::size_t>(kX0 + 2)] = 2;
    });
    CHECK(compare(frame, v, true, 3).differ == 0);
    CHECK(compare(frame, v, true, 1).differ > 0);
}

TEST_CASE("beam_compare: the frame's first line starting with the held word 8000")
{
    Vram v = stripes();
    v[16000] = 0x0F;                    // word 8000: 0000111100110101
    v[16001] = 0x35;
    const Vram* pv = &v;
    const Beam::Lines frame = beam_of(v, true, [pv](int y, std::vector<std::uint8_t>& ticks) {
        if (y != 0) return;
        for (int x = 0; x < 16; ++x) {
            const std::uint8_t byte = (*pv)[static_cast<std::size_t>(16000 + x / 8)];
            const bool lit = ((byte >> (7 - x % 8)) & 1) != 0;
            for (int k = 0; k < 8; ++k)
                ticks[static_cast<std::size_t>(kX0 + 8 * x + k)] =
                    static_cast<std::uint8_t>((lit ? (~kBorder & 7) : kBorder) | 8);
        }
    });
    CHECK(compare(frame, v, true, 3, -1, true).differ == 0);
    CHECK(compare(frame, v, true, 3).differ > 0);
}

namespace {

// 320 x 200 with each word's attributes `lead` ticks ahead of its points:
// a word's last ticks show the next word's ink or paper under the old
// point - a colour no point near has.  At the line's right end the next
// word is the next line's first (the frame's last line: word 8000), the
// video's address running on.
Beam::Lines attributes_ahead(const Vram& v, int lead)
{
    const Vram* pv = &v;
    return beam_of(v, false, [pv, lead](int y, std::vector<std::uint8_t>& ticks) {
        if (y < 0) return;
        for (int w = 1; w <= 40; ++w) {
            const int x = 16 * w - 1;       // the old word's last sample
            const std::size_t at = w < 40 ? static_cast<std::size_t>(y * 80 + w * 2 + 1)
                                 : y < 199 ? static_cast<std::size_t>((y + 1) * 80 + 1) : 16001;
            const std::uint8_t attr = (*pv)[at];
            const std::uint8_t pix = (*pv)[static_cast<std::size_t>(y * 80 + (x / 16) * 2)];
            const bool lit = (pix & 1) != 0;  // the old word's last point
            const int colour = (lit ? attr & 7 : (attr >> 3) & 7) | ((attr >> 6) & 1) << 3;
            for (int k = 1; k <= lead; ++k)
                ticks[static_cast<std::size_t>(kX0 + 8 * (x + 1) - k)] =
                    static_cast<std::uint8_t>(screen_detail::norm(colour));
        }
    });
}

}  // namespace

TEST_CASE("beam_compare: 320 x 200, the attributes 5 ticks ahead of the points pass within 7, not within 4")
{
    Vram v = attributes();
    v[16001] = 0x2C;                    // word 8000's attributes: ink 4, paper 5
    CHECK(compare(beam_of(v, false), v, false, 0).differ == 0);
    const Beam::Lines frame = attributes_ahead(v, 5);
    CHECK(compare(frame, v, false, 3, 7).differ == 0);
    CHECK(compare(frame, v, false, 3, 4).differ > 0);
}
