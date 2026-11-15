// beam_compare: a frame as the beam draws it (beam.h: the video DAC's
// inputs at every master tick) against the emulator's picture of its video
// memory (screen_compare.h: what each point must be).  Nothing is sampled:
// every tick of the 640 points' 5120 is compared, so a point cut short at a
// line's edge, a word a point late or a colour a few ticks wrong inside a
// run is seen, where a sample a point would take one tick for the point.
//
// The tolerance is in time, not in samples, and each output has its own:
// a tick passes when each of the DAC's four inputs (blue, red, green,
// brightness - each by its own path) stands at a level the picture gives
// it with its point taken anywhere within `points` ticks of the tick and
// its attribute within `attributes` - the two come to the DAC by paths of
// their own too; a flashing word in either of its phases.  Each is
// held to under half the smallest thing it draws, so that nothing can be
// lost, gained or moved by a whole point in it:
// - the points (and the border's edges), 3 ticks, 25 ns: under half a
//   640 x 200 point (67 ns).  A word one point off - the left-edge race
//   lost (docs/findings.md, D70) - fails tick by tick, no phase check
//   beside it;
// - the attributes (320 x 200), 7 ticks, 58 ns: under half a 320 x 200
//   point (133 ns), the least an attribute colours.  The board's
//   attributes come ahead of its points by their paths' difference: 3
//   ticks at the typical delays, 5 (42 ns) at the datasheets' maximums,
//   so a word's last ticks show the next word's colours under its last
//   point.
// Past the line's right end the attributes are the next word's, the
// video's address running on (the next line's first; the frame's last
// line's word 8000).  Beyond the area's ends the line may show the border
// or its blank (at 72 Hz there is no border: the blank meets the picture).
//
// `held`: the frame's first line starts with word 8000, the word Э3's
// registers held through the frame's border (screen_compare.h, `held`).
#pragma once

#include "beam.h"
#include "screen_compare.h"

#include <cstdint>
#include <vector>

constexpr int kBeamPoints = 3;          // master ticks: 25 ns
constexpr int kBeamAttributes = 7;      // 58 ns

struct BeamCompare {
    int x0 = -1, dy = -1;               // the area's first tick in a line, its first line
    int differ = -1;                    // ticks off the picture beyond the tolerance (-1: no fit)
    int first_x = -1, first_y = -1;     // the first of them: its 640 x 200 point and line
    int first_tick = -1;                // its tick within that point (0..7)
    int want = 0, got = 0;              // the picture's colour there and the beam's
};

namespace beam_detail {

constexpr int kAreaTicks = 640 * 8;

// the line's colour at ticks from..from+n-1 (before its first change, its
// first colour; after its last, its last)
inline std::vector<std::uint8_t> colours(const Beam::Line& line, int from, int n)
{
    std::vector<std::uint8_t> out(static_cast<std::size_t>(n), line.empty() ? 0 : line.front().colour);
    std::size_t next = 0;
    std::uint8_t colour = out.empty() ? 0 : out.front();
    for (int t = 0; t < n; ++t) {
        while (next < line.size() && line[next].tick <= from + t)
            colour = line[next++].colour;
        out[static_cast<std::size_t>(t)] = colour;
    }
    return out;
}

// the 640 x 200 point a tick from the area's first lies in (-1 before it)
inline int point_of(int t)
{
    return t < 0 ? -1 : t / 8;
}

// The levels each of the DAC's four inputs may take, as bits: an input has
// its own path, so a tick's colour may take each bit from another moment of
// the window - an edge where blue comes on a tick before green.
struct Levels {
    unsigned ones = 0, zeros = 0;

    void add(int c)                     // black's brightness may be either
    {
        const unsigned u = static_cast<unsigned>(c);
        const unsigned seen = (c & 7) == 0 ? 7u : 15u;
        ones |= (u & seen) | (~seen & 8u);
        zeros |= (~u & seen) | (~seen & 8u);
    }

    bool take(int s) const              // s as norm() leaves it
    {
        const unsigned u = static_cast<unsigned>(s);
        const unsigned seen = (s & 7) == 0 ? 7u : 15u;
        return ((u & seen & ~ones) | (~u & seen & ~zeros)) == 0;
    }
};

// past the line's end the attributes are the next word's, the video's
// address running on: the next line's first, the frame's last line's word
// 8000; the colours of point xp under them, both phases
inline void add_past_end(Levels phase[2], const std::uint8_t* vram, int xp, int y)
{
    const std::uint8_t attr = vram[y < 199 ? (y + 1) * 80 + 1 : 16001];
    const std::uint8_t pix = vram[y * 80 + xp / 32 * 2];
    const bool lit = ((pix >> (7 - (xp / 2) % 8)) & 1) != 0;
    const int bright = (attr >> 6) & 1, ink = attr & 7, paper = (attr >> 3) & 7;
    const int a = screen_detail::norm((lit ? ink : paper) | bright << 3);
    phase[0].add(a);
    phase[1].add((attr & 0x80) ? screen_detail::norm((lit ? paper : ink) | bright << 3) : a);
}

// what line y may show with its point taken at point xp and its attribute
// at xa, into `phase`: [0] the picture's first phase, [1] its second (a
// flashing word's ink and paper swapped).  Beyond the area's ends the
// border, whatever its brightness, or the line's blank (72 Hz: no border,
// the blank meets the picture).
inline void add_at(Levels phase[2], const std::uint8_t* vram, const std::uint8_t* held, bool hires, int border,
                   int xp, int xa, int y)
{
    if (xp < 0 || xp >= 640) {
        for (int k = 0; k < 2; ++k) {
            phase[k].add(0);
            phase[k].add(border & 7);
            phase[k].add((border & 7) | 8);
        }
        return;
    }
    if (!hires && xa >= 640) {
        add_past_end(phase, vram, xp, y);
        return;
    }
    const int xc = xa < 0 ? 0 : xa > 639 ? 639 : xa;
    const bool from_held = held && y == 0 && (hires ? xp < 16 : xp == 0);
    const std::uint8_t* sources[2] = {from_held && hires ? held : vram, from_held && !hires ? held : nullptr};
    for (const std::uint8_t* mem : sources) {
        if (!mem)
            continue;
        int a = 0, b = 0;
        screen_detail::expect(mem, hires, border, xp, xc, y, a, b);
        phase[0].add(a);
        phase[1].add(b);
        int c[3] = {0, 0, 0};
        const int n = screen_detail::candidates(mem, hires, border, xp, xc, y, c);
        if (n > (a != b ? 2 : 1)) {     // after a flashing word: its ink and paper swapped
            phase[0].add(c[n - 1]);
            phase[1].add(c[n - 1]);
        }
    }
}

// is colour s the picture's at tick t of line y, its point taken anywhere
// within `points` ticks and its attribute within `attributes` - the two
// come to the outputs by paths of their own - each of the DAC's inputs at
// a level it has somewhere there?
inline bool fits_near(const std::uint8_t* vram, const std::uint8_t* held, bool hires, int border, int t,
                      int points, int attributes, int y, int s)
{
    Levels phase[2];
    for (int xp = point_of(t - points); xp <= point_of(t + points); ++xp)
        for (int xa = point_of(t - attributes); xa <= point_of(t + attributes); ++xa)
            add_at(phase, vram, held, hires, border, xp, xa, y);
    return phase[0].take(s) || phase[1].take(s);
}

inline int count(const Beam::Lines& frame, const std::uint8_t* vram, const std::uint8_t* held, bool hires,
                 int border, int x0, int dy, int points, int attributes, int limit, BeamCompare* first)
{
    int n = 0;
    for (int y = 0; y < 200 && n <= limit; ++y) {
        const std::vector<std::uint8_t> line = colours(frame[static_cast<std::size_t>(dy + y)], x0, kAreaTicks);
        for (int t = 0; t < kAreaTicks && n <= limit; ++t) {
            const int s = screen_detail::norm(line[static_cast<std::size_t>(t)]);
            if (fits_near(vram, held, hires, border, t, points, attributes, y, s))
                continue;
            if (n == 0 && first) {
                int a = 0, b = 0;
                screen_detail::expect(vram, hires, border, t / 8, t / 8, y, a, b);
                first->first_x = t / 8;
                first->first_y = y;
                first->first_tick = t % 8;
                first->want = a;
                first->got = s;
            }
            ++n;
        }
    }
    return n;
}

}  // namespace beam_detail

// The area is found as screen_compare finds it, on the beam's points
// (Beam::samples), then to the tick: the first tick where the picture fits
// the beam best with no tolerance, within the point the samples gave.
inline BeamCompare compare_beam(const Beam::Lines& frame, const std::uint8_t* vram, bool hires, int border,
                                int x0 = 137, int reach = 16, int points = kBeamPoints,
                                int attributes = kBeamAttributes, bool held = false)
{
    BeamCompare c;
    const ScreenCompare s = compare_screen(Beam::samples(frame), vram, hires, border, x0, reach, 0, held);
    if (s.differ < 0)
        return c;
    const std::vector<std::uint8_t> held_vram = held ? screen_detail::with_held_word(vram)
                                                     : std::vector<std::uint8_t>();
    const std::uint8_t* h = held ? held_vram.data() : nullptr;
    const int base = Beam::grid_of(frame) - 8 + 8 * s.dx;
    int best = -1;
    for (int at = base - 4; at <= base + 4; ++at) {
        if (at < 0)
            continue;
        const int n = beam_detail::count(frame, vram, h, hires, border, at, s.dy, 0, 0, best < 0 ? 1 << 30 : best,
                                         nullptr);
        if (best < 0 || n < best) {
            best = n;
            c.x0 = at;
        }
    }
    c.dy = s.dy;
    c.differ = beam_detail::count(frame, vram, h, hires, border, c.x0, c.dy, points, attributes, 1 << 30, &c);
    return c;
}
