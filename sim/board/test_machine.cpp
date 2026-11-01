// The machine of the MiSTer core (rtl/ms0515.sv: the board inside the
// machine, rtl/board/ns4_machine.sv, with the drive and the picture as
// logic) against the bench's board (rtl/board/ns4.sv) and ns4_video on its
// lines, tick for tick from power-on, no disk: the picture, the drive's
// lines and the keyboard's line the same.  The picture comes a master tick
// later: inside the machine ns4_video takes the board's levels at the edge
// they change on, where the bench gives them to it after.
//
// With a keyboard, the bench's stub (keyboard_stub.h) sends keys: on the
// bench's board at XS2.3 as the socket has them, on the machine's serial
// line as a UART's (idle high), the inverse.
#include <doctest/doctest.h>

#include "Vms0515.h"
#include "Vms0515___024root.h"
#include "Vns4.h"
#include "Vns4_video.h"
#include "keyboard_stub.h"
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

struct Result {
    std::uint64_t pixels = 0, differ = 0;
    std::uint64_t rxd_falls = 0;        // ПРМ1 H's falls: the keys' bits at the USART
};

void idle(Vms0515& m, bool keyboard)
{
    m.reset = 0;
    m.kbd_on = keyboard ? 1 : 0;
    m.kbd_rx = 1;
    m.layout = 1;
    m.img_mounted = 0;
    m.img_readonly = 0;
    m.img_size = 0;
    m.sd_ack = 0;
    m.sd_buff_addr = 0;
    m.sd_buff_dout = 0;
    m.sd_buff_wr = 0;
}

void feed(Vns4_video& v, const Vns4& n)
{
    v.ts15 = n.n_TS15_H;
    v.blue = n.n_SC_H;
    v.red = n.n_KC_H;
    v.green = n.n_ZC_H;
    v.bright = n.nx_D69_9;
    v.ssi_n = n.n_SSI_L;
    v.ksi_n = n.n_KSI_L;
    v.sgi_n = n.n_SGI_L;
}

// 60 ms from power-on (three frames and more), side by side
Result compare(bool keyboard)
{
    auto board = std::make_unique<Vns4>();
    auto video = std::make_unique<Vns4_video>();
    auto machine = std::make_unique<Vms0515>();
    disconnect(*board);
    Vms0515& m = *machine;
    idle(m, keyboard);
    keyboard::Stub kbd;
    if (keyboard)
        kbd.press(1000ull * kMasterMhz, {0302, 0301, 0313, 0275});
    Result r;
    Picture before{};                   // the bench's picture a tick before
    bool rxd_was = true;
    const std::uint64_t ticks = 60000ull * kMasterMhz;
    for (std::uint64_t t = 0; t < ticks; ++t) {
        power(*board, t);
        if (keyboard) {
            const bool level = kbd.tick(t, board->n_VYHKL_L != 0);
            board->x_XS2_p3 = level ? 1 : 0;
            board->x_XS2_p3_oe = 1;
            m.kbd_rx = level ? 0 : 1;
        }
        tick(*board);
        const Vns4& n = *board;
        Vns4_video& v = *video;
        feed(v, n);
        tick(v);
        tick(m);
        r.pixels += v.ce_pix;
        const Picture now{m.ce_pix, m.red, m.green, m.blue, m.hs, m.vs, m.hblank, m.vblank};
        // the keyboard USART D22's RxD, ПРМ1 H, behind the receiver D48
        const bool rxd = m.rootp->ms0515__DOT__board__DOT__n_PRM1_H != 0;
        const bool same = now == before && m.kbd_tx == !n.n_VYHKL_L && rxd == (n.n_PRM1_H != 0) &&
                          m.sd_rd == 0 && m.sd_wr == 0;
        r.rxd_falls += rxd != rxd_was && !rxd ? 1 : 0;
        rxd_was = rxd;
        if (!same && ++r.differ <= 5)
            MESSAGE("tick " << t << ": the machine's ce " << int(m.ce_pix) << " r " << int(m.red) << " g "
                            << int(m.green) << " b " << int(m.blue) << ", the bench's ce " << int(before.ce) << " r "
                            << int(before.r) << " g " << int(before.g) << " b " << int(before.b) << "; keyboard "
                            << int(m.kbd_tx) << " / " << int(n.n_VYHKL_L) << ", sd_rd " << int(m.sd_rd));
        before = {v.ce_pix, v.r, v.g, v.b, v.hs, v.vs, v.hblank, v.vblank};
    }
    return r;
}

}  // namespace

TEST_CASE("ms0515: the machine's picture and lines from power-on, as the bench's board")
{
    const Result r = compare(false);
    MESSAGE(r.pixels << " pixels");
    CHECK(r.pixels > 3 * 260 * 960);
    CHECK(r.differ == 0);
}

TEST_CASE("ms0515: a keyboard on the serial line - keys sent, the board's line out")
{
    const Result r = compare(true);
    MESSAGE(r.rxd_falls << " falls of the USART's RxD");
    CHECK(r.pixels > 3 * 260 * 960);
    CHECK(r.rxd_falls >= 8);
    CHECK(r.differ == 0);
}
