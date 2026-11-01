// video_half: pixels made on the 120 MHz master clock come out on the
// 60 MHz video clock each once, in order, with their colour and syncs.
#include <doctest/doctest.h>

#include "Vvideo_half.h"

#include <cstdint>
#include <memory>
#include <vector>

namespace {

struct Pixel {
    int r, g, b, flags;
};

// one master tick: clk rises, clk_video with every other one
void tick(Vvideo_half& m, std::uint64_t t)
{
    m.clk = 0;
    if (t % 2 == 0) m.clk_video = 0;
    m.eval();
    m.clk = 1;
    if (t % 2 == 0) m.clk_video = 1;
    m.eval();
}

std::vector<Pixel> run(int phase, int pixels)
{
    auto model = std::make_unique<Vvideo_half>();
    Vvideo_half& m = *model;
    std::vector<Pixel> out;
    const std::uint64_t ticks = static_cast<std::uint64_t>(pixels) * 8 + 40;
    int sent = 0;
    for (std::uint64_t t = 0; t < ticks; ++t) {
        const bool now = t >= 16 && (t - static_cast<std::uint64_t>(phase)) % 8 == 0 && sent < pixels;
        m.ce_pix = now ? 1 : 0;
        if (now) {
            m.r = static_cast<CData>(sent * 3);
            m.g = static_cast<CData>(sent * 5);
            m.b = static_cast<CData>(sent * 7);
            m.hs = sent & 1;
            m.vs = (sent >> 1) & 1;
            m.hblank = (sent >> 2) & 1;
            m.vblank = (sent >> 3) & 1;
            ++sent;
        }
        const bool video_edge = t % 2 == 0;
        tick(m, t);
        if (video_edge && m.ce_out)
            out.push_back({m.r_out, m.g_out, m.b_out,
                           m.hs_out | m.vs_out << 1 | m.hblank_out << 2 | m.vblank_out << 3});
    }
    return out;
}

}  // namespace

TEST_CASE("video_half: every pixel once, in order, whatever the phase of the master tick it came on")
{
    for (const int phase : {0, 1, 2, 3}) {
        INFO("phase " << phase);
        const std::vector<Pixel> out = run(phase, 100);
        REQUIRE(out.size() == 100);
        for (int i = 0; i < 100; ++i) {
            CHECK(out[static_cast<std::size_t>(i)].r == (i * 3 & 0xFF));
            CHECK(out[static_cast<std::size_t>(i)].g == (i * 5 & 0xFF));
            CHECK(out[static_cast<std::size_t>(i)].b == (i * 7 & 0xFF));
            CHECK(out[static_cast<std::size_t>(i)].flags == (i & 15));
        }
    }
}
