// screen_compare: a frame taken off the board's video DAC inputs against
// the picture the emulator's video memory makes (its own Screen's rules,
// docs/hardware/video.md of the emulator): 320 x 200 with an attribute a
// word, or 640 x 200 in the border's colour and its complement.
//
// A board sample is 4 bits: blue 1, red 2, green 4 (СЦ, КЦ, ЗЦ H) and
// brightness 8 (D69.9); a sample a ТС15 tick, so 640 samples an active
// line and two a 320-mode point.  Where the active area sits in the frame
// is searched for, not assumed.  Flashing cells match either phase (the
// two count their own frames); a black point's brightness is not compared.
//
// `slack` 1 holds a sample to the picture within one sample: its point and
// its attribute each taken there or one sample to the left or right (beyond
// the area's ends the border stands).  The frame is sampled at the rises
// of ТС15 H, and the board's outputs do not change together: the
// attributes and the border's edges come some ticks before the points
// (docs/timing.md, "Corners").  At the typical delays the sampling tick
// has all of them on one side; at a corner it may fall between them, and
// an edge then shows a sample early or late - on a monitor, some tens of
// nanoseconds.  So a board at a corner is held to the picture within one
// sample, the typical one to the sample.
//
// `held`: the board as Э3 draws it, D70 off in the border.  Its registers
// keep through the frame's border the word they took at the last line's
// right edge - word 8000, the first past the 8000 of the screen - and the
// frame's first line starts with it: in 640 x 200 its first clock shifts
// that word out, so the first 16 points are word 8000's; in 320 x 200 the
// first clock loads the line's own word as D70 comes on, the points' gate
// already open, so only the frame's first sample is word 8000's first
// point (docs/findings.md, D70).  At a corner (`slack` 1) the 320 x 200
// line's first clock may shift as in 640 x 200 (the max corner's setup):
// the frame's first word then has word 8000's points in word 0's
// attributes, the attribute registers loading on their own clock.
#pragma once

#include <cstdint>
#include <vector>

struct ScreenCompare {
    int dx = -1, dy = -1;           // the active area's corner in the frame
    int differ = -1;                // samples that differ there (-1: no fit)
    int first_x = -1, first_y = -1; // the first of them, in 640 x 200
    int want = 0, got = 0;          // its expected and taken samples
    int phase_breaks = -1;          // lines whose first word lies off the rest's phase
};

namespace screen_detail {

inline int norm(int s)
{
    return (s & 7) == 0 ? 0 : s;    // black has no brightness to see
}

// the expected sample(s) with the point of sample xp and the attribute of
// sample xa (0..639), line y: two when it flashes
inline void expect(const std::uint8_t* vram, bool hires, int border, int xp, int xa, int y, int& a, int& b)
{
    const std::uint8_t* row = vram + y * 80;
    if (hires) {
        const int w = xp / 16, p = xp % 16;
        const std::uint8_t byte = p < 8 ? row[w * 2] : row[w * 2 + 1];
        const bool lit = ((byte >> (7 - (p % 8))) & 1) != 0;
        a = b = norm((lit ? (~border & 7) : (border & 7)) | 8);
        return;
    }
    const int px = xp / 2, p = px % 8;
    const std::uint8_t pix = row[px / 8 * 2], attr = row[xa / 16 * 2 + 1];
    const bool lit = ((pix >> (7 - p)) & 1) != 0;
    const int bright = (attr >> 6) & 1, fg = attr & 7, bg = (attr >> 3) & 7;
    a = norm((lit ? fg : bg) | bright << 3);
    b = (attr & 0x80) ? norm((lit ? bg : fg) | bright << 3) : a;
}

// is sample s the picture's with its point taken at xp and its attribute at
// xa?  A point beyond the line's ends is the border: its colour, whatever
// the brightness; an attribute beyond them is the end word's.
inline bool fits(const std::uint8_t* vram, bool hires, int border, int xp, int xa, int y, int s)
{
    if (xp < 0 || xp >= 640)
        return (s & 7) == (border & 7);
    int a = 0, b = 0;
    expect(vram, hires, border, xp, xa < 0 ? 0 : xa > 639 ? 639 : xa, y, a, b);
    if (s == a || s == b)
        return true;
    // The first sample of a word after a flashing one, in the flash's
    // inverted half: the inversion (ВК8 H, from D31 and D39) is let go a
    // couple of К555 delays after the attributes change, so for 25 ns the
    // new word shows its ink where its paper is and the other way round -
    // the board's own (docs/baseline.md, open list 3).
    if (!hires && xp % 16 == 0 && xp >= 16) {
        const std::uint8_t* row = vram + y * 80;
        if (row[(xp / 16 - 1) * 2 + 1] & 0x80) {
            const std::uint8_t attr = row[xp / 16 * 2 + 1];
            const int bright = (attr >> 6) & 1;
            const int ink = norm((attr & 7) | bright << 3), paper = norm(((attr >> 3) & 7) | bright << 3);
            return s == (a == ink ? paper : ink);
        }
    }
    return false;
}

// the video memory as the frame's first line shows it with the held word:
// word 0 replaced by word 8000 (its attributes too, or not)
inline std::vector<std::uint8_t> with_held_word(const std::uint8_t* vram, bool attributes = true)
{
    std::vector<std::uint8_t> v(vram, vram + 16384);
    v[0] = v[16000];
    if (attributes)
        v[1] = v[16001];
    return v;
}

inline int count(const std::vector<std::vector<std::uint8_t>>& frame, const std::uint8_t* vram, bool hires,
                 int border, int dx, int dy, int limit, int slack, ScreenCompare* first,
                 const std::uint8_t* held = nullptr, const std::uint8_t* held_points = nullptr)
{
    int n = 0;
    for (int y = 0; y < 200 && n <= limit; ++y) {
        const auto& line = frame[static_cast<std::size_t>(dy + y)];
        for (int x = 0; x < 640 && n <= limit; ++x) {
            const int s = norm(line[static_cast<std::size_t>(dx + x)]);
            // the held word: the first line's first word in 640 x 200, its
            // first sample in 320 x 200 (either word there)
            const bool from_held = held && y == 0 && (hires ? x < 16 : x == 0);
            const std::uint8_t* mem = from_held && hires ? held : vram;
            bool right = fits(mem, hires, border, x, x, y, s) ||
                         (from_held && !hires && fits(held, hires, border, x, x, y, s));
            for (int dp = -slack; dp <= slack && !right; ++dp)
                for (int da = -slack; da <= slack && !right; ++da)
                    right = fits(mem, hires, border, x + dp, x + da, y, s);
            // at a corner, 320 x 200's first word with word 8000's points
            const bool held_word = held_points && slack > 0 && !hires && y == 0 && x < 16;
            for (int dp = -slack; dp <= slack && !right && held_word; ++dp)
                for (int da = -slack; da <= slack && !right; ++da)
                    right = fits(held_points, hires, border, x + dp, x + da, y, s);
            if (!right) {
                if (n == 0 && first) {
                    int a = 0, b = 0;
                    expect(vram, hires, border, x, x, y, a, b);
                    first->first_x = x;
                    first->first_y = y;
                    first->want = a;
                    first->got = s;
                }
                ++n;
            }
        }
    }
    return n;
}

// the shift of the points (-1, 0 or 1 sample) at which samples x0..x1-1 of
// line y fit the picture best, to the sample; 2 when no one shift is best
inline int phase(const std::vector<std::uint8_t>& line, const std::uint8_t* vram, bool hires, int border, int dx,
                 int y, int x0, int x1)
{
    int miss[3] = {0, 0, 0};
    for (int x = x0; x < x1; ++x) {
        const int s = norm(line[static_cast<std::size_t>(dx + x)]);
        for (int d = -1; d <= 1; ++d)
            miss[d + 1] += fits(vram, hires, border, x - d, x, y, s) ? 0 : 1;
    }
    int best = 0;
    for (int d = -1; d <= 1; ++d)
        if (miss[d + 1] < miss[best + 1])
            best = d;
    for (int d = -1; d <= 1; ++d)
        if (d != best && miss[d + 1] == miss[best + 1])
            return 2;
    return best;
}

// The lines whose first word lies at another shift than the rest of the
// line: 640 x 200's left-edge race lost (docs/findings.md, D70), which
// `slack` passes edge by edge.  The frame's first line, with the held
// word, is left out; so is a line whose phase is not clear.  640 x 200
// only: in 320 x 200 a point is two samples, a shift of one is no phase,
// and the line's first clock has its own rule there (`held`).
inline int phase_breaks(const std::vector<std::vector<std::uint8_t>>& frame, const std::uint8_t* vram, bool hires,
                        int border, int dx, int dy)
{
    int n = 0;
    for (int y = 1; y < 200; ++y) {
        const auto& line = frame[static_cast<std::size_t>(dy + y)];
        const int first = phase(line, vram, hires, border, dx, y, 0, 16);
        const int rest = phase(line, vram, hires, border, dx, y, 16, 640);
        n += first != 2 && rest != 2 && first != rest ? 1 : 0;
    }
    return n;
}

}  // namespace screen_detail

// The best fit of the 640 x 200 area within the frame - at any line of it
// (the sync PROM's page puts the picture at line 35 of 260 at 60 Hz and
// lower in the 312 of 50 Hz), near x0 across - and what differs there
// (within `slack` samples).
inline ScreenCompare compare_screen(const std::vector<std::vector<std::uint8_t>>& frame,
                                    const std::uint8_t* vram, bool hires, int border, int x0 = 232,
                                    int reach = 16, int slack = 0, bool held = false)
{
    const std::vector<std::uint8_t> held_vram = held ? screen_detail::with_held_word(vram)
                                                     : std::vector<std::uint8_t>();
    const std::uint8_t* h = held ? held_vram.data() : nullptr;
    const std::vector<std::uint8_t> points_vram = held ? screen_detail::with_held_word(vram, false)
                                                       : std::vector<std::uint8_t>();
    const std::uint8_t* hp = held ? points_vram.data() : nullptr;
    ScreenCompare best;
    for (int dy = 0; dy + 200 <= static_cast<int>(frame.size()); ++dy) {
        for (int dx = x0 - reach; dx <= x0 + reach; ++dx) {
            bool fits = dx >= 0;
            for (int y = 0; y < 200 && fits; ++y)
                fits = static_cast<int>(frame[static_cast<std::size_t>(dy + y)].size()) >= dx + 640;
            if (!fits)
                continue;
            const int limit = best.differ < 0 ? 1 << 30 : best.differ;
            const int n = screen_detail::count(frame, vram, hires, border, dx, dy, limit, 0, nullptr, h);
            if (best.differ < 0 || n < best.differ) {
                best.differ = n;
                best.dx = dx;
                best.dy = dy;
            }
        }
    }
    // the area sits where the picture fits to the sample; the slack is
    // given there only, or a sample of it would go to the area's place too
    if (best.differ > 0)
        best.differ = screen_detail::count(frame, vram, hires, border, best.dx, best.dy, 1 << 30, slack, &best, h,
                                           hp);
    if (best.differ >= 0)
        best.phase_breaks = hires ? screen_detail::phase_breaks(frame, vram, hires, border, best.dx, best.dy) : 0;
    return best;
}
