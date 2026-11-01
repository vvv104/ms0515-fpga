// The board's picture through ns4_video (rtl/parts), the module the MiSTer
// core shows it with: from power-on, every pixel it puts out against the
// sample the bench's camera takes (sim/board/run_board.cpp) at the same
// place of the line before, and its frame blanking against the lines that
// had no sample unblanked.
#include <doctest/doctest.h>

#include "Vns4.h"
#include "Vns4_video.h"
#include "power.h"

#include <cstdint>
#include <deque>
#include <memory>
#include <utility>
#include <vector>

namespace {

constexpr int kMasterMhz = 120;
constexpr std::size_t kLineSamples = 960;

// a sample as the camera takes it, with the syncs and the blanking:
// bits 0 blue, 1 red, 2 green, 3 brightness, 4 ССИ L, 5 КСИ L, 6 СГИ L
std::uint8_t sample(const Vns4& m)
{
    return static_cast<std::uint8_t>((m.n_SC_H & 1u) | (m.n_KC_H & 1u) << 1 | (m.n_ZC_H & 1u) << 2 |
                                     (m.nx_D69_9 & 1u) << 3 | (m.n_SSI_L & 1u) << 4 | (m.n_KSI_L & 1u) << 5 |
                                     (m.n_SGI_L & 1u) << 6);
}

std::uint8_t level(std::uint8_t s, int bit)
{
    const bool blank = (s & 0x40) == 0;
    return blank || !(s >> bit & 1) ? 0 : (s & 8) ? 255 : 170;
}

}  // namespace

TEST_CASE("ns4_video: the board's lines from power-on, each out a line later as the camera took it")
{
    auto board = std::make_unique<Vns4>();
    auto video = std::make_unique<Vns4_video>();
    disconnect(*board);
    std::vector<std::vector<std::uint8_t>> lines;
    std::deque<std::pair<std::size_t, std::size_t>> taken;       // (line, sample) of each sample taken
    std::vector<bool> blank;                                     // a line done had no sample unblanked
    bool ssi_was = true, ts15_was = false;
    std::size_t pixels = 0, frame_blank = 0, differ = 0;
    const std::uint64_t ticks = 60000ull * kMasterMhz;           // 60 ms: three frames and more
    for (std::uint64_t t = 0; t < ticks; ++t) {
        power(*board, t);
        board->clk = 0;
        board->eval();
        board->clk = 1;
        board->eval();
        const Vns4& m = *board;
        // the camera: a line from the ССИ L fall, a sample at each ТС15 H rise
        if (!m.n_SSI_L && ssi_was) lines.emplace_back();
        ssi_was = m.n_SSI_L != 0;
        const bool rise = m.n_TS15_H && !ts15_was;
        ts15_was = m.n_TS15_H != 0;
        if (rise && !lines.empty() && lines.back().size() < kLineSamples) {
            taken.emplace_back(lines.size() - 1, lines.back().size());
            lines.back().push_back(sample(m));
        }
        // the module on the same levels, its master tick after the board's
        Vns4_video& v = *video;
        v.ts15 = m.n_TS15_H;
        v.blue = m.n_SC_H;
        v.red = m.n_KC_H;
        v.green = m.n_ZC_H;
        v.bright = m.nx_D69_9;
        v.ssi_n = m.n_SSI_L;
        v.ksi_n = m.n_KSI_L;
        v.sgi_n = m.n_SGI_L;
        v.clk = 0;
        v.eval();
        v.clk = 1;
        v.eval();
        if (!v.ce_pix || taken.empty()) continue;
        const auto [line, x] = taken.front();
        taken.pop_front();
        if (line < 2) continue;                                  // the first line's before is not known
        const std::vector<std::uint8_t>& before = lines[line - 1];
        REQUIRE(x < before.size());
        const std::uint8_t s = before[x];
        while (blank.size() < line) {                            // the lines done: unblanked anywhere?
            bool none = true;
            for (const std::uint8_t b : lines[blank.size()])
                none = none && (b & 0x40) == 0;
            blank.push_back(none);
        }
        const bool all_blank = blank[line - 1];
        ++pixels;
        frame_blank += v.vblank ? 1 : 0;
        const bool same = v.b == level(s, 0) && v.r == level(s, 1) && v.g == level(s, 2) &&
                          v.hs == !(s & 0x10) && v.vs == !(s & 0x20) && v.hblank == !(s & 0x40) &&
                          (v.vblank != 0) == all_blank;
        if (!same && ++differ <= 5)
            MESSAGE("line " << line - 1 << " sample " << x << ": the camera's " << int(s) << ", out r " << int(v.r)
                            << " g " << int(v.g) << " b " << int(v.b) << " hs " << int(v.hs) << " vs "
                            << int(v.vs) << " hblank " << int(v.hblank) << " vblank " << int(v.vblank));
    }
    MESSAGE(pixels << " pixels, " << frame_blank << " of them in the frame's blanking");
    CHECK(pixels > 3 * 260 * kLineSamples);
    CHECK(frame_blank > 0);
    CHECK(frame_blank < pixels / 2);
    CHECK(differ == 0);
}
