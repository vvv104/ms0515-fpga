// ns4_video: the board's picture for MiSTer's video interface.  The samples
// the bench's camera takes (sim/board/run_board.cpp) - the levels at each
// ТС15 H rise, lines from the ССИ L fall - come out a line later, a pixel
// enable each, with the syncs, and the blanking split into the line's and
// the frame's: a line with no sample unblanked is the frame's.
#include <doctest/doctest.h>

#include "Vns4_video.h"

#include <cstdint>
#include <memory>
#include <vector>

namespace {

constexpr int kSamples = 960;           // a line: 64 us at 15 MHz
constexpr int kTicks = 8;               // a sample: 8 master ticks (15 MHz of 120)

// what the board puts out at one sample
struct In {
    bool blue, red, green, bright, ssi_n, ksi_n, sgi_n;
};

// what comes out with a pixel enable
struct Out {
    std::uint8_t r, g, b;
    bool hs, vs, hblank, vblank;
};

class Bench {
public:
    Bench()
    {
        m_.clk = 0;
        m_.ts15 = 0;
        m_.ssi_n = 1;
        m_.ksi_n = 1;
        m_.sgi_n = 0;
        m_.eval();
    }

    // one sample: ТС15 H high for half of it, rising at its start
    void sample(const In& in)
    {
        for (int k = 0; k < kTicks; ++k) {
            m_.blue = in.blue;
            m_.red = in.red;
            m_.green = in.green;
            m_.bright = in.bright;
            m_.ssi_n = in.ssi_n;
            m_.ksi_n = in.ksi_n;
            m_.sgi_n = in.sgi_n;
            m_.ts15 = k < kTicks / 2;
            tick();
        }
    }

    std::vector<Out> out;

private:
    void tick()
    {
        m_.clk = 0;
        m_.eval();
        m_.clk = 1;
        m_.eval();
        if (m_.ce_pix)
            out.push_back({m_.r, m_.g, m_.b, m_.hs != 0, m_.vs != 0, m_.hblank != 0, m_.vblank != 0});
    }

    std::unique_ptr<Vns4_video> model_ = std::make_unique<Vns4_video>();
    Vns4_video& m_ = *model_;
};

// a line: sync at its start, blanked but for samples 100..899 unless the
// whole line is blanked; the colours a pattern of the line and the sample
In level(int line, int x, bool blanked)
{
    const int v = (line * 7 + x) % 16;
    return {(v & 1) != 0, (v & 2) != 0, (v & 4) != 0, (v & 8) != 0, x >= 70, line >= 3, !blanked && x >= 100 && x < 900};
}

std::uint8_t expect(bool on, bool bright, bool blanked)
{
    return blanked || !on ? 0 : bright ? 255 : 170;
}

}  // namespace

TEST_CASE("ns4_video: a line out a line later, a pixel enable a sample")
{
    Bench bench;
    const int lines = 6;
    const auto blanked = [](int line) { return line < 2; };     // the frame's blanking
    for (int line = 0; line < lines; ++line)
        for (int x = 0; x < kSamples; ++x)
            bench.sample(level(line, x, blanked(line)));
    // every sample gave a pixel; the first line's are the line before it
    REQUIRE(bench.out.size() >= static_cast<std::size_t>((lines - 1) * kSamples));
    for (int line = 0; line + 1 < lines; ++line) {
        for (int x = 0; x < kSamples; ++x) {
            const Out& o = bench.out[static_cast<std::size_t>((line + 1) * kSamples + x)];
            const In in = level(line, x, blanked(line));
            INFO("line " << line << " sample " << x);
            CHECK(o.hs == !in.ssi_n);
            CHECK(o.vs == !in.ksi_n);
            CHECK(o.hblank == !in.sgi_n);
            CHECK(o.vblank == blanked(line));
            CHECK(o.b == expect(in.blue, in.bright, !in.sgi_n));
            CHECK(o.r == expect(in.red, in.bright, !in.sgi_n));
            CHECK(o.g == expect(in.green, in.bright, !in.sgi_n));
        }
    }
}
