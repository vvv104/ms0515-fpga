// frame_grab: the board's picture taken off its video DAC's digital inputs -
// a line from each fall of ССИ L, a point from each rise of ТС15 H to the
// next: blue 1, red 2, green 4 (СЦ, КЦ, ЗЦ H), brightness 8 (D69.9); a
// frame ends as КСИ L falls.  The whole line, blanking too: up to 960
// points, 64 us at 15 MHz.
//
// A point's colour is the one its master ticks held longest, as a monitor
// shows it: the board's colour changes a few ticks into the point, and at
// a 320 x 200 word's edge the next word's attributes reach the outputs a
// few ticks before its points (their path through the К555КП2's data
// inputs is shorter than the points' through its select) - a spike no
// screen shows, which a sample at one instant would take for the point.
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

constexpr std::size_t kLineSamples = 960;

class FrameGrab {
public:
    using Lines = std::vector<std::vector<std::uint8_t>>;
    Lines lines;        // the frame being taken
    Lines last;         // the frame that ended last

    // true as a frame ends (then in `last`); the tick's samples go to the next
    template <class Model>
    bool see(const Model& m)
    {
        bool ended = false;
        if (!m.n_KSI_L && ksi_was_) {
            last = std::exchange(lines, {});
            ended = true;
        }
        ksi_was_ = m.n_KSI_L != 0;
        if (!m.n_SSI_L && ssi_was_) {
            lines.emplace_back();
            open_ = false;
        }
        ssi_was_ = m.n_SSI_L != 0;
        if (m.n_TS15_H && !ts15_was_ && !lines.empty()) {
            if (open_ && lines.back().size() < kLineSamples)
                lines.back().push_back(held_longest());
            ticks_.fill(0);
            open_ = true;
        }
        ts15_was_ = m.n_TS15_H != 0;
        if (open_)
            ++ticks_[(m.n_SC_H & 1u) | (m.n_KC_H & 1u) << 1 | (m.n_ZC_H & 1u) << 2 | (m.nx_D69_9 & 1u) << 3];
        return ended;
    }

private:
    std::uint8_t held_longest() const
    {
        std::size_t best = 0;
        for (std::size_t v = 1; v < ticks_.size(); ++v)
            if (ticks_[v] > ticks_[best]) best = v;
        return static_cast<std::uint8_t>(best);
    }

    std::array<int, 16> ticks_{};   // the ticks each colour held in the open point
    bool open_ = false;             // a point under way (from a rise of ТС15 H)
    bool ksi_was_ = true, ssi_was_ = true, ts15_was_ = false;
};
