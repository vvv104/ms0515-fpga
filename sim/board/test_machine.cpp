// The machine of the MiSTer core (rtl/ms0515.sv: the board inside the
// machine with the memory expansion module, rtl/board/ns4_ext_machine.sv,
// with the drive and the picture as logic) against the bench's board
// (rtl/board/ns4.sv) and ns4_video on its lines, tick for tick from
// power-on, no disk, the module taken out: the picture, the drive's lines
// and the keyboard's line the same.  The picture comes a master tick
// later: inside the machine ns4_video takes the board's levels at the edge
// they change on, where the bench gives them to it after.
//
// With a keyboard, the bench's stub (keyboard_stub.h) sends keys: on the
// bench's board at XS2.3 as the socket has them, on the machine's serial
// line as a UART's (idle high), the inverse.
//
// With the module in, its RAM disk in DDR3 (rtl/parts/ext_ram.sv, the
// bench's DDR3 ddram.h) is worked by exdisk.rom as on the bench's board.
#include <doctest/doctest.h>

#include "Vms0515.h"
#include "Vms0515___024root.h"
#include "Vns4.h"
#include "Vns4_video.h"
#include "cpu_writes.h"
#include "ddram.h"
#include "keyboard_stub.h"
#include "power.h"

#include <algorithm>
#include <cstdint>
#include <fstream>
#include <iterator>
#include <memory>
#include <optional>
#include <string>
#include <vector>

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
    m.rom_we = 0;
    m.rom_addr = 0;
    m.rom_data = 0;
    m.d70_drawn = 0;
    m.frame = 0;                        // 72 Hz, E3 alone
    m.kbd_source = keyboard ? 2 : 0;    // the serial line, or nothing
    m.ps2_key = 0;
    m.kbd_mode = 0;
    m.kbd_rx = 1;
    m.layout = 1;
    m.img_mounted = 0;
    m.img_readonly = 0;
    m.img_size = 0;
    m.sd_ack = 0;
    m.sd_buff_addr = 0;
    m.sd_buff_dout = 0;
    m.sd_buff_wr = 0;
    m.module_in = 0;
    m.ddr_busy = 0;
    m.ddr_dout_ready = 0;
    m.ddr_dout = 0;
}

// a master tick of the machine with its DDR3
void tick(Vms0515& m, Ddram& ddr, std::uint64_t& t)
{
    ddr.present(m, t);
    m.clk = 0;
    m.eval();
    ddr.take(m, t);
    m.clk = 1;
    m.eval();
    ++t;
}

// a 16 KB ROM from firmware/tests into the machine's sockets, DDR3 there
// from the first tick, as on the DE10-Nano
void load_rom(Vms0515& m, Ddram& ddr, std::uint64_t& t, const char* name)
{
    std::ifstream f(std::string(MS0515_FIRMWARE_DIR "/tests/") + name, std::ios::binary);
    REQUIRE(f);
    const std::vector<char> rom{std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>()};
    REQUIRE(rom.size() == 16384);
    tick(m, ddr, t);                    // the chips' cells from their CONTENTS first
    for (unsigned i = 0; i < 16384; ++i) {
        m.rom_we = 1;
        m.rom_addr = static_cast<SData>(i);
        m.rom_data = static_cast<CData>(rom[i]);
        tick(m, ddr, t);
    }
    m.rom_we = 0;
}

// the bus as cpu_writes.h reads it, from the nets inside the machine's
// board (АД00 is not among them: nothing there reads it, and the writes
// are taken by the word)
struct Bus {
    unsigned n_PVAS_L, n_PVAK_L, n_PMLB_L, n_PSTB_L;
    unsigned n_AD00_H, n_AD01_H, n_AD02_H, n_AD03_H, n_AD04_H, n_AD05_H, n_AD06_H, n_AD07_H;
    unsigned n_AD08_H, n_AD09_H, n_AD10_H, n_AD11_H, n_AD12_H, n_AD13_H, n_AD14_H, n_AD15_H;
    unsigned n_MK00_H, n_MK01_H, n_MK02_H, n_MK03_H, n_MK04_H, n_MK05_H, n_MK06_H, n_MK07_H;
    unsigned n_MK08_H, n_MK09_H, n_MK10_H, n_MK11_H, n_MK12_H, n_MK13_H, n_MK14_H, n_MK15_H;
};

Bus bus_of(const Vms0515& m)
{
#define MS0515_NET(n) m.rootp->ms0515__DOT__board__DOT__##n
    return {MS0515_NET(n_PVAS_L), MS0515_NET(n_PVAK_L), MS0515_NET(n_PMLB_L), MS0515_NET(n_PSTB_L),
            0, MS0515_NET(n_AD01_H), MS0515_NET(n_AD02_H), MS0515_NET(n_AD03_H),
            MS0515_NET(n_AD04_H), MS0515_NET(n_AD05_H), MS0515_NET(n_AD06_H), MS0515_NET(n_AD07_H),
            MS0515_NET(n_AD08_H), MS0515_NET(n_AD09_H), MS0515_NET(n_AD10_H), MS0515_NET(n_AD11_H),
            MS0515_NET(n_AD12_H), MS0515_NET(n_AD13_H), MS0515_NET(n_AD14_H), MS0515_NET(n_AD15_H),
            MS0515_NET(n_MK00_H), MS0515_NET(n_MK01_H), MS0515_NET(n_MK02_H), MS0515_NET(n_MK03_H),
            MS0515_NET(n_MK04_H), MS0515_NET(n_MK05_H), MS0515_NET(n_MK06_H), MS0515_NET(n_MK07_H),
            MS0515_NET(n_MK08_H), MS0515_NET(n_MK09_H), MS0515_NET(n_MK10_H), MS0515_NET(n_MK11_H),
            MS0515_NET(n_MK12_H), MS0515_NET(n_MK13_H), MS0515_NET(n_MK14_H), MS0515_NET(n_MK15_H)};
#undef MS0515_NET
}

// exdisk.rom (firmware/tests/exdisk.mac, as test_exdisk.cpp runs it on the
// bench's board) on the machine with the module in or out, its RAM in a
// DDR3 that answers in 12..48 ticks (100-400 ns) and holds a request one
// tick in four; until the program's end mark or 200 ms.  faults: ext_ram's
// from the supply's rise on - before it the board's strobes are its
// power-on's, while the supply monitor holds it (power.h), and nothing
// reads what they show
CpuWrites run_exdisk(Vms0515& m, Ddram& ddr, bool module_in, std::uint32_t& faults)
{
    idle(m, false);
    m.module_in = module_in ? 1 : 0;
    std::uint64_t t = 0;
    load_rom(m, ddr, t, "exdisk.rom");
    CpuWrites writes;
    std::uint32_t at_rise = 0;
    while (t < 200000ull * kMasterMhz && !writes.word_at(01012)) {
        tick(m, ddr, t);
        writes.see(bus_of(m));
        if (t == kSupplyRiseTicks)
            at_rise = m.ram_faults;
    }
    faults = m.ram_faults - at_rise;
    return writes;
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

// The machine is generated at the typical delays only (tools/board.py's
// corner makes the bench's board): beside a board at a corner it is another
// machine, and the two comparisons are passed over there.
#ifdef MS0515_DELAY_CORNER
constexpr bool kCorner = true;
#else
constexpr bool kCorner = false;
#endif

TEST_CASE("ms0515: the machine's picture and lines from power-on, as the bench's board" * doctest::skip(kCorner))
{
    const Result r = compare(false);
    MESSAGE(r.pixels << " pixels");
    CHECK(r.pixels > 3 * 260 * 960);
    CHECK(r.differ == 0);
}

TEST_CASE("ms0515: a keyboard on the serial line - keys sent, the board's line out" * doctest::skip(kCorner))
{
    const Result r = compare(true);
    MESSAGE(r.rxd_falls << " falls of the USART's RxD");
    CHECK(r.pixels > 3 * 260 * 960);
    CHECK(r.rxd_falls >= 8);
    CHECK(r.differ == 0);
}

TEST_CASE("ms0515: the МС 7004 inside - a PC key's code reaches the board's USART")
{
    // after the keyboard's power-on beeps (some 130 ms) the PC's A is
    // pressed (by character; ЛАТ dark, which the translator reads as РУС:
    // Ф/F, 302); its frame comes to D22's RxD - read there as the USART
    // reads it: idle high, a start bit low, eight bits from bit 0, 4800 baud
    auto machine = std::make_unique<Vms0515>();
    Vms0515& m = *machine;
    idle(m, false);
    m.kbd_source = 1;
    const std::uint64_t press = 200000ull * kMasterMhz;
    const std::uint64_t bit = 1000000ull * kMasterMhz / 4800;
    std::uint64_t start = 0;
    int byte = -1;
    bool rxd_was = true;
    for (std::uint64_t t = 0; t < press + 100000ull * kMasterMhz; ++t) {
        if (t == press) m.ps2_key = static_cast<SData>(1 << 10 | 1 << 9 | 0x1C);  // A down
        tick(m);
        const bool rxd = m.rootp->ms0515__DOT__board__DOT__n_PRM1_H != 0;
        if (t > press && start == 0 && rxd_was && !rxd) {
            start = t;
            byte = 0;
        }
        for (int b = 0; b < 8 && start != 0; ++b)
            if (t == start + bit / 2 + static_cast<std::uint64_t>(b + 1) * bit && rxd)
                byte |= 1 << b;
        rxd_was = rxd;
    }
    MESSAGE("the USART's RxD after the key: " << byte);
    CHECK(byte == 0302);
}

TEST_CASE("ms0515: another ROM loaded into the sockets - the even bytes to D65, the odd to D77")
{
    auto machine = std::make_unique<Vms0515>();
    Vms0515& m = *machine;
    idle(m, false);
    tick(m);                            // the chips' cells from their CONTENTS first
    const auto byte = [](unsigned i) { return static_cast<CData>((i * 7u + 3u) ^ (i >> 8)); };
    for (unsigned i = 0; i < 16384; ++i) {
        m.rom_we = 1;
        m.rom_addr = static_cast<SData>(i);
        m.rom_data = byte(i);
        tick(m);
    }
    m.rom_we = 0;
    tick(m);
    int wrong = 0;
    for (unsigned i = 0; i < 8192; ++i) {
        wrong += m.rootp->ms0515__DOT__board__DOT__D65__DOT__cells[i] != byte(2 * i) ? 1 : 0;
        wrong += m.rootp->ms0515__DOT__board__DOT__D77__DOT__cells[i] != byte(2 * i + 1) ? 1 : 0;
    }
    CHECK(wrong == 0);
}

TEST_CASE("ms0515: the processor's request stands two ticks and more when its bus takes it")
{
    // the core acts on the bus's events two ticks late and the bus reads
    // its request a tick before it flags the take; on the board's 8 MHz
    // crystal, with the board's READY, the request read has stood at least
    // two ticks - boards/de10nano/kr1807vm1.sdc gives the path two
    auto machine = std::make_unique<Vms0515>();
    Vms0515& m = *machine;
    idle(m, false);
    std::uint64_t acted = 0, closest = ~0ull, takes = 0;
    bool seen = false;
    for (std::uint64_t t = 0; t < 30000ull * kMasterMhz; ++t) {
        tick(m);
        if (m.rootp->ms0515__DOT__board__DOT__D83__DOT__take && seen) {
            ++takes;
            closest = std::min(closest, t - acted - 1);
        }
        if (m.rootp->ms0515__DOT__board__DOT__D83__DOT__core__DOT__late != 0) {
            acted = t;
            seen = true;
        }
    }
    MESSAGE(takes << " takes, the request held at least " << closest << " ticks");
    CHECK(takes > 10000);
    CHECK(closest >= 2);
}

// The speaker comes out of the machine: the node of D93's open collectors
// (XP7.2 through R61).  With the test ROM firmware/tests/sound.rom in the
// sockets the channel 2 tone, 1 kHz, reaches it: rises 1000 us apart.
TEST_CASE("ms0515: the speaker's node comes out - the timer's 1 kHz tone of the sound test ROM")
{
    std::ifstream f(MS0515_FIRMWARE_DIR "/tests/sound.rom", std::ios::binary);
    REQUIRE(f);
    const std::vector<char> rom{std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>()};
    REQUIRE(rom.size() == 16384);
    auto machine = std::make_unique<Vms0515>();
    Vms0515& m = *machine;
    idle(m, false);
    tick(m);
    for (unsigned i = 0; i < 16384; ++i) {
        m.rom_we = 1;
        m.rom_addr = static_cast<SData>(i);
        m.rom_data = static_cast<CData>(rom[i]);
        tick(m);
    }
    m.rom_we = 0;
    std::vector<std::uint64_t> rises;
    CData was = m.speaker;
    for (std::uint64_t t = 0; t < 100000ull * kMasterMhz; ++t) {
        tick(m);
        if (m.speaker && !was) rises.push_back(t);
        was = m.speaker;
    }
    int at_1khz = 0;
    for (std::size_t i = 1; i < rises.size(); ++i) {
        const double us = static_cast<double>(rises[i] - rises[i - 1]) / kMasterMhz;
        at_1khz += us > 999.0 && us < 1001.0 ? 1 : 0;
    }
    CHECK(at_1khz >= 10);
}

// The memory expansion module in the machine (plan 6a, step 4): its RAM
// disk in DDR3 (rtl/parts/ext_ram.sv) in place of its sixteen chips,
// exdisk's eight pages written through the data port and read back as on
// the bench's board with the chips (test_exdisk.cpp) - and no read came
// before the word it reads.
TEST_CASE("ms0515: the module's RAM disk in DDR3 - eight pages come back as written")
{
    auto machine = std::make_unique<Vms0515>();
    Ddram ddr(12, 48, 4);
    std::uint32_t faults = 0;
    const CpuWrites writes = run_exdisk(*machine, ddr, true, faults);
    REQUIRE(writes.word_at(01012) == std::optional<std::uint16_t>(052525));
    CHECK(writes.word_at(01010) == std::optional<std::uint16_t>(2048));
    CHECK(writes.word_at(01006) == std::optional<std::uint16_t>(0));
    CHECK(faults == 0);
    CHECK(ddr.writes() >= 2048);
    MESSAGE(ddr.reads() << " words fetched, " << ddr.writes() << " bytes written");
}

// Taken out in the menu, the module answers nothing (its select held off)
// and D80.5 is back on Э3's net: НС4's own registers at those addresses.
TEST_CASE("ms0515: the module taken out - the pages do not come back, DDR3 not written")
{
    auto machine = std::make_unique<Vms0515>();
    Ddram ddr(12, 48, 4);
    std::uint32_t faults = 0;
    const CpuWrites writes = run_exdisk(*machine, ddr, false, faults);
    REQUIRE(writes.word_at(01012) == std::optional<std::uint16_t>(052525));
    CHECK(writes.word_at(01006) != std::optional<std::uint16_t>(0));
    CHECK(ddr.writes() == 0);
}
