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
#pragma once

#include <cstdint>
#include <vector>

struct ScreenCompare {
    int dx = -1, dy = -1;           // the active area's corner in the frame
    int differ = -1;                // samples that differ there (-1: no fit)
    int first_x = -1, first_y = -1; // the first of them, in 640 x 200
    int want = 0, got = 0;          // its expected and taken samples
};

namespace screen_detail {

inline int norm(int s)
{
    return (s & 7) == 0 ? 0 : s;    // black has no brightness to see
}

// the expected sample(s) at point x (0..639), line y: two when it flashes
inline void expect(const std::uint8_t* vram, bool hires, int border, int x, int y, int& a, int& b)
{
    const std::uint8_t* row = vram + y * 80;
    if (hires) {
        const int w = x / 16, p = x % 16;
        const std::uint8_t byte = p < 8 ? row[w * 2] : row[w * 2 + 1];
        const bool lit = ((byte >> (7 - (p % 8))) & 1) != 0;
        a = b = norm((lit ? (~border & 7) : (border & 7)) | 8);
        return;
    }
    const int px = x / 2, w = px / 8, p = px % 8;
    const std::uint8_t pix = row[w * 2], attr = row[w * 2 + 1];
    const bool lit = ((pix >> (7 - p)) & 1) != 0;
    const int bright = (attr >> 6) & 1, fg = attr & 7, bg = (attr >> 3) & 7;
    a = norm((lit ? fg : bg) | bright << 3);
    b = (attr & 0x80) ? norm((lit ? bg : fg) | bright << 3) : a;
}

inline int count(const std::vector<std::vector<std::uint8_t>>& frame, const std::uint8_t* vram, bool hires,
                 int border, int dx, int dy, int limit, ScreenCompare* first)
{
    int n = 0;
    for (int y = 0; y < 200 && n <= limit; ++y) {
        const auto& line = frame[static_cast<std::size_t>(dy + y)];
        for (int x = 0; x < 640 && n <= limit; ++x) {
            int a = 0, b = 0;
            expect(vram, hires, border, x, y, a, b);
            const int s = norm(line[static_cast<std::size_t>(dx + x)]);
            if (s != a && s != b) {
                if (n == 0 && first) {
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

}  // namespace screen_detail

// The best fit of the 640 x 200 area within the frame near (x0, y0).
inline ScreenCompare compare_screen(const std::vector<std::vector<std::uint8_t>>& frame,
                                    const std::uint8_t* vram, bool hires, int border, int x0 = 232, int y0 = 35,
                                    int reach = 16)
{
    ScreenCompare best;
    for (int dy = y0 - reach; dy <= y0 + reach; ++dy) {
        if (dy < 0 || dy + 200 > static_cast<int>(frame.size()))
            continue;
        for (int dx = x0 - reach; dx <= x0 + reach; ++dx) {
            bool fits = dx >= 0;
            for (int y = 0; y < 200 && fits; ++y)
                fits = static_cast<int>(frame[static_cast<std::size_t>(dy + y)].size()) >= dx + 640;
            if (!fits)
                continue;
            const int limit = best.differ < 0 ? 1 << 30 : best.differ;
            const int n = screen_detail::count(frame, vram, hires, border, dx, dy, limit, nullptr);
            if (best.differ < 0 || n < best.differ) {
                best.differ = n;
                best.dx = dx;
                best.dy = dy;
            }
        }
    }
    if (best.differ > 0)
        screen_detail::count(frame, vram, hires, border, best.dx, best.dy, 1 << 30, &best);
    return best;
}
