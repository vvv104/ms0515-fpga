// The machine of the MiSTer core (rtl/ms0515.sv: the board inside the
// machine, rtl/board/ns4_machine.sv, with the drive and the picture as
// logic) against the bench's board (rtl/board/ns4.sv) and ns4_video on its
// lines, tick for tick from power-on: no disk, no keyboard - the picture,
// the drive's lines and the keyboard's line the same.  The picture comes a
// master tick later: inside the machine ns4_video takes the board's levels
// at the edge they change on, where the bench gives them to it after.
#include <doctest/doctest.h>

#include "Vms0515.h"
#include "Vns4.h"
#include "Vns4_video.h"
#include "power.h"

#include <cstdint>
#include <memory>

namespace {

constexpr int kMasterMhz = 120;

template <class V>
void tick(V& m)
{
    m.clk = 0;
    m.eval();
    m.clk = 1;
    m.eval();
}

// what ns4_video puts out in a tick
struct Picture {
    int ce, r, g, b, hs, vs, hblank, vblank;
    bool operator==(const Picture&) const = default;
};

}  // namespace

TEST_CASE("ms0515: the machine's picture and lines from power-on, as the bench's board")
{
    auto board = std::make_unique<Vns4>();
    auto video = std::make_unique<Vns4_video>();
    auto machine = std::make_unique<Vms0515>();
    disconnect(*board);
    Vms0515& m = *machine;
    m.reset = 0;
    m.kbd_in = 0;
    m.kbd_in_on = 0;
    m.layout = 1;
    m.img_mounted = 0;
    m.img_readonly = 0;
    m.img_size = 0;
    m.sd_ack = 0;
    m.sd_buff_addr = 0;
    m.sd_buff_dout = 0;
    m.sd_buff_wr = 0;
    std::uint64_t pixels = 0, differ = 0;
    Picture before{};                   // the bench's picture a tick before
    const std::uint64_t ticks = 60000ull * kMasterMhz;           // 60 ms: three frames and more
    for (std::uint64_t t = 0; t < ticks; ++t) {
        power(*board, t);
        tick(*board);
        const Vns4& n = *board;
        Vns4_video& v = *video;
        v.ts15 = n.n_TS15_H;
        v.blue = n.n_SC_H;
        v.red = n.n_KC_H;
        v.green = n.n_ZC_H;
        v.bright = n.nx_D69_9;
        v.ssi_n = n.n_SSI_L;
        v.ksi_n = n.n_KSI_L;
        v.sgi_n = n.n_SGI_L;
        tick(v);
        tick(m);
        pixels += v.ce_pix;
        const Picture now{m.ce_pix, m.red, m.green, m.blue, m.hs, m.vs, m.hblank, m.vblank};
        const bool same = now == before && m.kbd_out == n.n_VYHKL_L && m.sd_rd == 0 && m.sd_wr == 0;
        if (!same && ++differ <= 5)
            MESSAGE("tick " << t << ": the machine's ce " << int(m.ce_pix) << " r " << int(m.red) << " g "
                            << int(m.green) << " b " << int(m.blue) << ", the bench's ce " << int(before.ce) << " r "
                            << int(before.r) << " g " << int(before.g) << " b " << int(before.b) << "; keyboard "
                            << int(m.kbd_out) << " / " << int(n.n_VYHKL_L) << ", sd_rd " << int(m.sd_rd));
        before = {v.ce_pix, v.r, v.g, v.b, v.hs, v.vs, v.hblank, v.vblank};
    }
    MESSAGE(pixels << " pixels");
    CHECK(pixels > 3 * 260 * 960);
    CHECK(differ == 0);
}
