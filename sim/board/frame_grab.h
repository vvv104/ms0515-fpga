// frame_grab: the board's picture taken off its video DAC's digital inputs -
// a line from each fall of ССИ L, a sample at each rise of ТС15 H: blue 1,
// red 2, green 4 (СЦ, КЦ, ЗЦ H), brightness 8 (D69.9); a frame ends as КСИ L
// falls.  The whole line, blanking too: 960 samples, 64 us at 15 MHz.
#pragma once

#include "Vns4.h"

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
    bool see(const Vns4& m)
    {
        bool ended = false;
        if (!m.n_KSI_L && ksi_was_) {
            last = std::exchange(lines, {});
            ended = true;
        }
        ksi_was_ = m.n_KSI_L != 0;
        if (!m.n_SSI_L && ssi_was_) lines.emplace_back();
        ssi_was_ = m.n_SSI_L != 0;
        if (m.n_TS15_H && !ts15_was_ && !lines.empty() && lines.back().size() < kLineSamples)
            lines.back().push_back(static_cast<std::uint8_t>((m.n_SC_H & 1u) | (m.n_KC_H & 1u) << 1 |
                                                             (m.n_ZC_H & 1u) << 2 | (m.nx_D69_9 & 1u) << 3));
        ts15_was_ = m.n_TS15_H != 0;
        return ended;
    }

private:
    bool ksi_was_ = true, ssi_was_ = true, ts15_was_ = false;
};
