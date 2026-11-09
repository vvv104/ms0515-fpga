// The parts of the board the machine's software leaves alone, each worked
// by a test ROM (firmware/tests, tools/test_roms.py) run from reset in
// place of the machine's ROM, the bench on the connector it leads to.
#include <doctest/doctest.h>
#include <verilated.h>

#include "Vns4.h"
#include "cpu_writes.h"
#include "frame_grab.h"
#include "power.h"
#include "rom_socket.h"
#include "screen_compare.h"

extern "C" {
#include <ms0515/core/board.h>
#include <ms0515/core/cpu.h>
}

#include <cstdint>
#include <fstream>
#include <functional>
#include <iterator>
#include <memory>
#include <string>
#include <vector>

namespace {

constexpr std::uint64_t kMasterMhz = 120;

// A board built at a corner of the chips' delays (docs/timing.md).
#ifdef MS0515_DELAY_CORNER
constexpr bool kCorner = true;
#else
constexpr bool kCorner = false;
#endif

std::vector<std::uint8_t> read_rom(const std::string& name)
{
    std::ifstream f(std::string(MS0515_FIRMWARE_DIR) + "/tests/" + name, std::ios::binary);
    REQUIRE(f);
    return {std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>()};
}

// The board from power-on with a test ROM in the sockets; `plug` puts what
// the bench has on the connectors before each tick.
class Bench {
public:
    explicit Bench(const std::string& rom) : m_(std::make_unique<Vns4>())
    {
        Verilated::randReset(0);
        disconnect(*m_);
        REQUIRE(plug_rom(*m_, read_rom(rom)));
    }

    void run_ms(std::uint64_t ms, const std::function<void(Vns4&)>& plug)
    {
        const std::uint64_t end = t_ + ms * 1000 * kMasterMhz;
        while (t_ < end) {
            power(*m_, t_++);
            plug(*m_);
            m_->clk = 0;
            m_->eval();
            m_->clk = 1;
            m_->eval();
            writes.see(*m_);
        }
    }

    CpuWrites writes;

private:
    std::unique_ptr<Vns4> m_;
    std::uint64_t t_ = 0;
};

}  // namespace

// XP5 through a loop-back plug: circuit 103 (the line out, D20.7) back into
// 104 (XP5.3, D48's receiver), 108 (D20.6, the C2 USART's DTR, to XP5.5
// through E2) into 107 (XP5.6), its CTS and DSR.  (The net named СТЫК С2
// 108 is another: D28.6 from the keyboard USART's DTR to XS2.5 through SA1.)
// The ROM starts at about 70 ms, after СТАРТ; the five bytes take some 4 ms.
TEST_CASE("С2: five bytes through a loop-back plug on XP5 come back as sent, DSR from DTR")
{
    Bench b("c2loop.rom");
    b.run_ms(80, [](Vns4& m) {
        m.x_XP5_p3 = m.n_STYK_S2_103;
        m.x_XP5_p3_oe = 1;
        m.x_XP5_p6 = m.nx_D20_6;
        m.x_XP5_p6_oe = 1;
    });
    REQUIRE(b.writes.word_at(01012) == std::optional<std::uint16_t>(052525));
    CHECK(b.writes.word_at(01006) == std::optional<std::uint16_t>(0));     // no timeouts
    const std::uint8_t sent[] = {0125, 0252, 0, 0377, 0123};
    for (unsigned i = 0; i < 5; ++i) {
        INFO("byte " << i);
        CHECK(b.writes.byte(01000 + i) == std::optional<std::uint8_t>(sent[i]));
    }
    CHECK((*b.writes.word_at(01010) & 0200) != 0);         // DSR
}

// The MS 7007 port through a plug: port A of D82 (ДКЛ00..07, XP3.2..9) back
// into port B (ДКЛ08..15, XP8.10..17), its bits 0..2 into port C
// (ДКЛ16..18, XP8.18..20).  Each byte the program puts on A is read back
// on B and C, and the plug's lines are what A drives.
TEST_CASE("МС 7007 port: port A's bytes come back on ports B and C through a plug on XP3 and XP8")
{
    Bench b("p7007.rom");
    b.run_ms(80, [](Vns4& m) {
        const CData a[8] = {m.n_DKL00_L, m.n_DKL01_L, m.n_DKL02_L, m.n_DKL03_L,
                            m.n_DKL04_L, m.n_DKL05_L, m.n_DKL06_L, m.n_DKL07_L};
        CData* const in[11] = {&m.x_XP8_p10, &m.x_XP8_p11, &m.x_XP8_p12, &m.x_XP8_p13, &m.x_XP8_p14, &m.x_XP8_p15,
                               &m.x_XP8_p16, &m.x_XP8_p17, &m.x_XP8_p18, &m.x_XP8_p19, &m.x_XP8_p20};
        CData* const oe[11] = {&m.x_XP8_p10_oe, &m.x_XP8_p11_oe, &m.x_XP8_p12_oe, &m.x_XP8_p13_oe,
                               &m.x_XP8_p14_oe, &m.x_XP8_p15_oe, &m.x_XP8_p16_oe, &m.x_XP8_p17_oe,
                               &m.x_XP8_p18_oe, &m.x_XP8_p19_oe, &m.x_XP8_p20_oe};
        for (int i = 0; i < 11; ++i) {
            *in[i] = a[i % 8];
            *oe[i] = 1;
        }
    });
    REQUIRE(b.writes.word_at(01026) == std::optional<std::uint16_t>(052525));
    const std::uint8_t out[] = {1, 2, 4, 010, 020, 040, 0100, 0200, 0125, 0252};
    for (unsigned i = 0; i < 10; ++i) {
        INFO("byte " << i << ": " << int(out[i]));
        CHECK(b.writes.byte(01000 + 2 * i) == std::optional<std::uint8_t>(out[i]));
        CHECK((*b.writes.byte(01001 + 2 * i) & 7) == (out[i] & 7));
    }
    CHECK(b.writes.byte(01024) == std::optional<std::uint8_t>(0252));
}

// The speaker's node (D93's open collectors, through R61 to XP7.2): with the
// timer's channel 2 gated and the way open it carries the channel's square
// wave (1 kHz from 2 MHz / 2000); with the gate off it follows the control
// bit, inverted; with the way shut it rests high (but for D93's delay as
// the register changes: 19 ns typical, 2 ticks; 30 ns at the most, 4 -
// netlist/delays.csv, delays-max.csv).
TEST_CASE("sound: the timer's tone and the control bit reach the speaker on XP7")
{
    Bench b("sound.rom");
    std::vector<std::uint64_t> tone_rises;
    int bit_edges = 0, node_edges = 0, low_run = 0, longest_low = 0;
    std::uint64_t t = 0;
    CData node_was = 1, upr_was = 0;
    b.run_ms(110, [&](Vns4& m) {
        ++t;
        const CData node = m.nx_D93_3;
        if (m.n_RAZR_GR_H && m.n_SINT_H && !m.n_UPR_GR_H && node && !node_was) tone_rises.push_back(t);
        if (m.n_RAZR_GR_H && !m.n_SINT_H) {
            bit_edges += m.n_UPR_GR_H != upr_was;
            node_edges += node != node_was;
        }
        low_run = !m.n_RAZR_GR_H && !node ? low_run + 1 : 0;
        longest_low = low_run > longest_low ? low_run : longest_low;
        node_was = node;
        upr_was = m.n_UPR_GR_H;
    });
    REQUIRE(b.writes.word_at(01000) == std::optional<std::uint16_t>(052525));
    REQUIRE(tone_rises.size() > 5);
    const double period_us = static_cast<double>(tone_rises.back() - tone_rises.front()) /
                             static_cast<double>(tone_rises.size() - 1) / kMasterMhz;
    CHECK(period_us == doctest::Approx(1000.0).epsilon(0.001));
    CHECK(bit_edges >= 19);
    CHECK(node_edges == bit_edges);
    INFO("the node low with the way shut for " << longest_low << " ticks at the longest");
    CHECK(longest_low <= (kCorner ? 4 : 3));        // D93's delay and no longer
}

// The memory dispatcher: a tag in each of the 7 banks of the main and the
// extended RAM at every address line's offset, read back - 182 cells, every
// one its own (the program counts; the bench reads the counts).
TEST_CASE("memory: the dispatcher's banks, main and extended, each 8 KB of its own on every address line")
{
    Bench b("banks.rom");
    b.run_ms(100, [](Vns4&) {});
    REQUIRE(b.writes.word_at(01012) == std::optional<std::uint16_t>(052525));
    CHECK(b.writes.word_at(01010) == std::optional<std::uint16_t>(182));
    CHECK(b.writes.word_at(01006) == std::optional<std::uint16_t>(0));
}

namespace {

// The emulator from reset with the same test ROM, run until the program
// stands on its last BR . - its video memory is then the whole picture.
std::unique_ptr<ms0515_board_t> emulator_after(const std::string& rom_name)
{
    const std::vector<std::uint8_t> rom = read_rom(rom_name);
    auto emu = std::make_unique<ms0515_board_t>();
    board_init(emu.get());
    board_load_rom(emu.get(), rom.data(), static_cast<std::uint32_t>(rom.size()));
    board_reset(emu.get());
    for (int i = 0; i < 5'000'000; ++i) {
        const std::uint16_t pc = emu->cpu.r[7];
        cpu_step(&emu->cpu);
        if (emu->cpu.r[7] == pc) break;
    }
    return emu;
}

// The picture of a test ROM that draws and stops: a frame taken off the
// board's video DAC inputs a whole frame after the program's end mark,
// against the emulator's picture of its own video memory, point for point.
// `drawn`: the board as 3.858.420 Э3 draws it, D70's enable on ~D14.3, not
// on GND as on the owner's board (netlist/vvv104.csv).  A board at a corner
// of the delays is held to the picture within one sample (screen_compare.h,
// which also knows the board's 25 ns of ink before paper at a word's edge
// after a flashing one - docs/baseline.md, open list 3).  Э3's board starts
// its frame with the word its registers held through the frame's border,
// word 8000 (screen_compare.h, `held`).
void picture_as_emulator(const std::string& rom_name, std::uint64_t ms, bool drawn = false)
{
    Bench b(rom_name);
    FrameGrab grab;
    FrameGrab::Lines taken;
    int frames_after = 0;
    b.run_ms(ms, [&](Vns4& m) {
        m.s_D70_15 = drawn ? 1 : 0;
        if (grab.see(m) && b.writes.word_at(01000) == std::optional<std::uint16_t>(052525) && ++frames_after == 2)
            taken = grab.last;
    });
    REQUIRE(frames_after >= 2);
    const auto emu = emulator_after(rom_name);
    const ScreenCompare c = compare_screen(taken, board_get_vram(emu.get()), board_is_hires(emu.get()),
                                           board_get_border_color(emu.get()), 232, 16, kCorner ? 1 : 0, drawn);
    INFO("area at " << c.dx << ", " << c.dy << "; first at " << c.first_x << ", " << c.first_y << ": " << c.got
                    << ", the emulator's " << c.want);
    CHECK(c.differ == 0);
}

}  // namespace

// 320 x 200: points and attributes, every attribute (bright, flashing) once
// at least, through the window at 040000; the border 3.
TEST_CASE("video: 320 x 200 with every attribute, as the emulator's picture of the same memory")
{
    picture_as_emulator("vid320.rom", 420);
}

// 640 x 200: a bit a point in the border's colour (5) and its complement,
// through the window at 100000.
TEST_CASE("video: 640 x 200 in the border's colour, as the emulator's picture of the same memory")
{
    picture_as_emulator("vid640.rom", 420);
}

// The same two pictures on Э3's board, the serial machines' (D70 off in
// the border, its clocks stopped): its first clock at a line's left edge
// shifts the word the registers took at the right border, D70 coming on
// before that clock falls with the chips as fitted (netlist/fitted-1991.csv)
// and the К555ИР16's setup of the corner (tools/board.py, SETUP_NS); the
// frame's first word is the one held through the frame's border, word 8000
// (docs/findings.md, D70).
TEST_CASE("video: Э3's board draws 320 x 200 as the emulator")
{
    picture_as_emulator("vid320.rom", 420, true);
}

// With C70 on БОРДС L (netlist/analog.csv, "rc") the typical chain to D70's
// enable loses the race, so Э3's board takes the serial boards' faster one
// (tools/board.py, SERIAL_SPEED, fitted to the race's ~33 ns window).  That
// window is narrower than the corners' spread: at a corner the edge may be
// lost, and the test may fail there (docs/findings.md, D70).
TEST_CASE("video: Э3's board draws 640 x 200 as the emulator" * doctest::may_fail(kCorner))
{
    picture_as_emulator("vid640.rom", 420, true);
}

// The interrupts the machine's software leaves alone: C2's transmitter and
// receiver through the plug on XP5, the МС 7007's key detector with a key
// held on its matrix - row 0 (ДКЛ00, port A bit 0) pulls column 0 (ДКЛ08,
// XP8.10) low through the key's diode.  Each comes at the vector of the
// technical description's Table 4: 114, 110, 060.
TEST_CASE("interrupts: С2's transmitter and receiver and the МС 7007's key come at 114, 110 and 060")
{
    Bench b("irqs.rom");
    b.run_ms(260, [](Vns4& m) {
        m.x_XP5_p3 = m.n_STYK_S2_103;
        m.x_XP5_p3_oe = 1;
        m.x_XP5_p6 = m.nx_D20_6;
        m.x_XP5_p6_oe = 1;
        m.x_XP8_p10 = 0;
        m.x_XP8_p10_oe = !m.n_DKL00_L;
    });
    REQUIRE(b.writes.word_at(01000) == std::optional<std::uint16_t>(052525));
    CHECK(b.writes.word_at(01100) == std::optional<std::uint16_t>(0114));
    CHECK(b.writes.word_at(01102) == std::optional<std::uint16_t>(0110));
    CHECK(b.writes.word_at(01104) == std::optional<std::uint16_t>(060));
    CHECK_FALSE(b.writes.word_at(01106).has_value());
}
