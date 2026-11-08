// screen_compare on made-up frames: the exact comparison, and the one a
// board at a corner of its delays is held to - each sample as the picture's
// there or one sample to its left or right (docs/timing.md, "Corners").
#include <doctest/doctest.h>

#include "screen_compare.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace {

constexpr int kBorder = 5;
constexpr int kLeft = 232, kTop = 35;

using Frame = std::vector<std::vector<std::uint8_t>>;
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

// the frame a board would give of it: 260 lines of 960 samples, the border
// round the picture, a point lit in the border's complement
Frame frame_of(const Vram& v)
{
    Frame f(260, std::vector<std::uint8_t>(960, static_cast<std::uint8_t>(kBorder | 8)));
    for (int y = 0; y < 200; ++y)
        for (int x = 0; x < 640; ++x) {
            const std::uint8_t byte = v[static_cast<std::size_t>(y * 80 + x / 8)];
            const bool lit = ((byte >> (7 - x % 8)) & 1) != 0;
            f[static_cast<std::size_t>(kTop + y)][static_cast<std::size_t>(kLeft + x)] =
                static_cast<std::uint8_t>((lit ? (~kBorder & 7) : kBorder) | 8);
        }
    return f;
}

std::uint8_t& at(Frame& f, int x, int y)
{
    return f[static_cast<std::size_t>(kTop + y)][static_cast<std::size_t>(kLeft + x)];
}

}  // namespace

TEST_CASE("screen_compare: the picture itself differs nowhere and is found where it sits")
{
    const Vram v = stripes();
    const ScreenCompare c = compare_screen(frame_of(v), v.data(), true, kBorder);
    CHECK(c.differ == 0);
    CHECK(c.dx == kLeft);
    CHECK(c.dy == kTop);
}

TEST_CASE("screen_compare: a sample taken a sample early or late differs exactly, not within one sample")
{
    const Vram v = stripes();
    Frame f = frame_of(v);
    at(f, 4, 10) = at(f, 3, 10);            // the edge after four lit points, a sample late
    at(f, 7, 20) = at(f, 8, 20);            // the next word's first point, a sample early
    at(f, 0, 40) = static_cast<std::uint8_t>(kBorder | 8);      // the border, a sample late
    const ScreenCompare exact = compare_screen(f, v.data(), true, kBorder);
    CHECK(exact.differ == 3);
    CHECK(exact.first_x == 4);
    CHECK(exact.first_y == 10);
    CHECK(compare_screen(f, v.data(), true, kBorder, 232, 16, 1).differ == 0);
}

// 320 x 200: every word's points 01010101; the words' attributes by turns
// red on blue and green on yellow, so a point and an attribute taken from
// neighbouring samples give a colour neither sample has
TEST_CASE("screen_compare: an attribute a sample early is the picture within one sample")
{
    Vram v{};
    for (std::size_t w = 0; w < v.size() / 2; ++w) {
        v[w * 2] = 0x55;
        v[w * 2 + 1] = w % 2 == 0 ? 0x0A : 0x34;       // fg 2 on bg 1; fg 4 on bg 6
    }
    Frame f(260, std::vector<std::uint8_t>(960, 3));
    for (int y = 0; y < 200; ++y)
        for (int x = 0; x < 640; ++x) {
            const std::size_t w = static_cast<std::size_t>(y * 40 + x / 16);
            const bool lit = ((v[w * 2] >> (7 - x / 2 % 8)) & 1) != 0;
            const int attr = v[w * 2 + 1];
            at(f, x, y) = static_cast<std::uint8_t>(lit ? attr & 7 : (attr >> 3) & 7);
        }
    REQUIRE(compare_screen(f, v.data(), false, 3).differ == 0);
    at(f, 15, 10) = 4;                      // word 0's last point, lit, in word 1's colour
    CHECK(compare_screen(f, v.data(), false, 3).differ == 1);
    CHECK(compare_screen(f, v.data(), false, 3, 232, 16, 1).differ == 0);
}

TEST_CASE("screen_compare: a sample of no neighbour's value differs within one sample too")
{
    const Vram v = stripes();
    Frame f = frame_of(v);
    at(f, 1, 10) = 1;                       // amid four lit points: neither neighbour is this
    at(f, 5, 20) = at(f, 3, 20);            // lit two samples from the lit ones: 4 and 6 are dark
    const ScreenCompare c = compare_screen(f, v.data(), true, kBorder, 232, 16, 1);
    CHECK(c.differ == 2);
    CHECK(c.first_x == 1);
    CHECK(c.first_y == 10);
}

namespace {

// the frame of a 320 x 200 picture, each sample as the comparison's own
// rule has it (its first phase when flashing)
Frame frame_of_320(const Vram& v)
{
    Frame f(260, std::vector<std::uint8_t>(960, static_cast<std::uint8_t>(kBorder)));
    for (int y = 0; y < 200; ++y)
        for (int x = 0; x < 640; ++x) {
            int a = 0, b = 0;
            screen_detail::expect(v.data(), false, kBorder, x, x, y, a, b);
            at(f, x, y) = static_cast<std::uint8_t>(a);
        }
    return f;
}

// 320 x 200: points 10101010, ink 2 (red) on paper 1 (blue), steady
Vram cells()
{
    Vram v{};
    for (std::size_t i = 0; i < v.size(); i += 2) {
        v[i] = 0xAA;
        v[i + 1] = 0x0A;
    }
    return v;
}

}  // namespace

// Э3's board, D70 off in the border: the registers keep through the frame's
// border the word they took at the last line's right edge - word 8000, the
// first past the screen (docs/findings.md, D70).  In 640 x 200 the frame's
// first line's first clock shifts it out: its first word is word 8000.
TEST_CASE("screen_compare: Э3's frame starts with word 8000 in 640 x 200")
{
    Vram v = stripes();
    v[16000] = 0x0F;
    v[16001] = 0x35;
    Frame f = frame_of(v);
    for (int x = 0; x < 16; ++x) {
        const std::uint8_t byte = v[static_cast<std::size_t>(16000 + x / 8)];
        const bool lit = ((byte >> (7 - x % 8)) & 1) != 0;
        at(f, x, 0) = static_cast<std::uint8_t>((lit ? (~kBorder & 7) : kBorder) | 8);
    }
    CHECK(compare_screen(f, v.data(), true, kBorder, kLeft, 16, 0, true).differ == 0);
    CHECK(compare_screen(f, v.data(), true, kBorder, kLeft, 16, 0, false).differ > 0);
    // a later line's first word is its own
    at(f, 0, 1) = static_cast<std::uint8_t>(~at(f, 0, 1) & 15);
    CHECK(compare_screen(f, v.data(), true, kBorder, kLeft, 16, 0, true).differ == 1);
}

// In 320 x 200 the first clock loads the line's own word as D70 comes on,
// but the points' gate is open by then: the first sample of the frame shows
// word 8000's first point, and only that one.
TEST_CASE("screen_compare: Э3's frame shows word 8000 in its first 320 x 200 sample only")
{
    Vram v = cells();
    v[16000] = 0x00;
    v[16001] = 0x26;   // paper 4 (green)
    Frame f = frame_of_320(v);
    at(f, 0, 0) = 4;
    CHECK(compare_screen(f, v.data(), false, kBorder, kLeft, 16, 0, true).differ == 0);
    CHECK(compare_screen(f, v.data(), false, kBorder, kLeft, 16, 0, false).differ == 1);
    at(f, 1, 0) = 4;
    CHECK(compare_screen(f, v.data(), false, kBorder, kLeft, 16, 0, true).differ == 1);
}

// At a corner of the delays (the max corner's К555ИР16 setup and paths)
// the 320 x 200 line's first clock may shift as in 640 x 200: the frame's
// first word then shows word 8000's points in word 0's attributes (the
// attribute registers load on their own clock), within one sample.
TEST_CASE("screen_compare: at a corner Э3's 320 x 200 frame may show word 8000's points in its first word")
{
    Vram v = cells();
    v[16000] = 0x55;   // points 01010101: the cells' complement
    v[16001] = 0x26;
    Frame f = frame_of_320(v);
    for (int x = 0; x < 16; ++x)
        at(f, x, 0) = static_cast<std::uint8_t>(at(f, x ^ 2, 0));   // each point as its neighbour's
    CHECK(compare_screen(f, v.data(), false, kBorder, kLeft, 16, 1, true).differ == 0);
    CHECK(compare_screen(f, v.data(), false, kBorder, kLeft, 16, 0, true).differ > 0);
    CHECK(compare_screen(f, v.data(), false, kBorder, kLeft, 16, 1, false).differ > 0);
}
