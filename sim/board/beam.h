// beam: the board's picture as the monitor's beam draws it - the video DAC's
// digital inputs (blue 1, red 2, green 4 from СЦ, КЦ, ЗЦ H; brightness 8
// from D69.9) at every master tick, kept as the ticks they change at.  A
// line from each fall of ССИ L, a frame ending as КСИ L falls, as
// FrameGrab takes them; but no point is sampled: a colour held for three
// ticks is three ticks of the line, not a point or nothing.  What the
// frames are drawn from (tools/beam_to_video.py), so that a point narrowed
// at an edge shows narrowed, and the dynamic picture is the board's own.
#pragma once

#include <array>
#include <cstdint>
#include <ostream>
#include <utility>
#include <vector>

class Beam {
public:
    struct Change {
        std::uint16_t tick;     // master ticks from the line's start
        std::uint8_t colour;
    };
    using Line = std::vector<Change>;
    using Lines = std::vector<Line>;
    Lines lines;        // the frame being taken
    Lines last;         // the frame that ended last

    // true as a frame ends (then in `last`)
    template <class Model>
    bool see(const Model& m)
    {
        bool ended = false;
        if (!m.n_KSI_L && ksi_was_) {
            last = std::exchange(lines, {});
            ended = true;
        }
        ksi_was_ = m.n_KSI_L != 0;
        const std::uint8_t colour = static_cast<std::uint8_t>((m.n_SC_H & 1u) | (m.n_KC_H & 1u) << 1 |
                                                              (m.n_ZC_H & 1u) << 2 | (m.nx_D69_9 & 1u) << 3);
        if (!m.n_SSI_L && ssi_was_) {
            lines.emplace_back();
            tick_ = 0;
            lines.back().push_back({0, colour});
        }
        else if (!lines.empty() && tick_ < kMaxTick) {
            ++tick_;
            if (colour != colour_)
                lines.back().push_back({tick_, colour});
        }
        ssi_was_ = m.n_SSI_L != 0;
        colour_ = colour;
        return ended;
    }

    // A frame's points as a monitor shows them: a sample every 8 ticks
    // (a 640 x 200 point), its colour the one held longest, the windows on
    // the output's own grid - the tick (mod 8) the colour most often
    // changes at - not on ТС15 H's rises, which lead the points by the
    // points' path to the outputs (a monitor sees the outputs alone).  A
    // line is `line_ticks` long, 64 us; the first window ends on the grid.
    static std::vector<std::vector<std::uint8_t>> samples(const Lines& frame, int line_ticks = 7680)
    {
        const int grid = grid_of(frame);
        std::vector<std::vector<std::uint8_t>> out;
        for (const Line& line : frame) {
            out.emplace_back();
            std::size_t next = 0;
            std::uint8_t colour = 0;
            for (int from = grid - 8; from < line_ticks; from += 8) {
                std::array<int, 16> held{};
                for (int t = from < 0 ? 0 : from; t < from + 8 && t < line_ticks; ++t) {
                    while (next < line.size() && line[next].tick <= t)
                        colour = line[next++].colour;
                    ++held[colour];
                }
                std::size_t best = 0;
                for (std::size_t v = 1; v < held.size(); ++v)
                    if (held[v] > held[best]) best = v;
                out.back().push_back(static_cast<std::uint8_t>(best));
            }
        }
        return out;
    }

    // the output's own grid: the tick (mod 8) the colour most often changes at
    static int grid_of(const Lines& frame)
    {
        std::array<int, 8> at{};
        for (const Line& line : frame)
            for (std::size_t i = 1; i < line.size(); ++i)
                ++at[line[i].tick % 8];
        int grid = 0;
        for (int k = 1; k < 8; ++k)
            if (at[static_cast<std::size_t>(k)] > at[static_cast<std::size_t>(grid)]) grid = k;
        return grid;
    }

    // a frame as a file: the line count, then each line's change count and
    // changes (tick, colour), little-endian 16-bit counts and ticks
    static void write(std::ostream& out, const Lines& frame)
    {
        put16(out, frame.size());
        for (const Line& line : frame) {
            put16(out, line.size());
            for (const Change& c : line) {
                put16(out, c.tick);
                out.put(static_cast<char>(c.colour));
            }
        }
    }

private:
    static constexpr std::uint16_t kMaxTick = 0xFFFF;

    static void put16(std::ostream& out, std::size_t v)
    {
        out.put(static_cast<char>(v & 0xFF));
        out.put(static_cast<char>((v >> 8) & 0xFF));
    }

    std::uint16_t tick_ = 0;
    std::uint8_t colour_ = 0;
    bool ksi_was_ = true, ssi_was_ = true;
};
