#pragma once
// The frame rate a build's jumpers give (netlist/variants.md): E3 ties
// РВК1 Н to ground, E4 РВК0 Н, and the two pick the sync PROMs' page.  The
// generated board has E3 fitted and E4 not - build -01, 72 Hz, as on every
// НС4 photographed; its inputs s_E3 (1 takes E3 off) and s_E4 (1 fits E4)
// give the other builds.  The emulator reads the same two lines at 177602
// bits 3 and 4 off its dip_refresh, and times its frame by it.
#include <string>

struct FrameRate {
    int hz;
    unsigned char s_E3, s_E4;   // the board's jumper switches
    unsigned char dip_refresh;  // the emulator's: bit 0 РВК0, bit 1 РВК1
    int lines;                  // a frame
    double line_us;
    int picture_x;              // the 640 points' first sample in a line (frame_grab.h)
};

// 72 (the default), 50 or 60; nullptr for any other figure
inline const FrameRate* frame_rate(int hz)
{
    static const FrameRate rates[] = {
        {72, 0, 0, 1, 262, 160.0 / 3.0, 137},   // E3 alone: РВК1 0, РВК0 1
        {50, 0, 1, 0, 312, 64.0, 232},          // E3 and E4: both 0
        {60, 1, 1, 2, 260, 64.0, 232},          // E4 alone: РВК1 1, РВК0 0
    };
    for (const FrameRate& r : rates)
        if (r.hz == hz) return &r;
    return nullptr;
}

inline const FrameRate& default_frame_rate() { return *frame_rate(72); }

// a board model's jumpers set as `r` says
template <class Board>
void set_frame_rate(Board& m, const FrameRate& r)
{
    m.s_E3 = r.s_E3;
    m.s_E4 = r.s_E4;
}
