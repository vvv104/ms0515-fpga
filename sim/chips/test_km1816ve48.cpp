// КМ1816ВЕ48 (8035): the MCS-48 at its pins, against the MCS-48 User's
// Manual (1980) - docs/km1816ve48.md.  The bench is the МС 7004's
// arrangement: the program in memory outside, the low address latched off
// BUS as ALE falls (D4), the high bits off P20..P23, the byte on BUS while
// PSEN is low (D5); the crystal a half-period every 13 master ticks.
#include <doctest/doctest.h>

#include "Vkm1816ve48.h"
#include "Vkm1816ve48___024root.h"

#include <array>
#include <cstdint>
#include <memory>
#include <vector>

namespace {

class Bench {
public:
    std::array<std::uint8_t, 4096> rom{};
    std::uint8_t latched = 0;
    int t1 = 1, int_n = 1;
    std::uint64_t ticks = 0;

    Bench() { m_.p4 = 0; m_.p6 = 1; m_.p1 = 1; m_.p39 = 1; m_.p2 = 0; }

    Vkm1816ve48& m() { return m_; }
    auto* r() { return m_.rootp; }

    void reset_release() { m_.p4 = 1; }

    std::uint8_t bus_out() const
    {
        return static_cast<std::uint8_t>(m_.p12 | m_.p13 << 1 | m_.p14 << 2 | m_.p15 << 3 | m_.p16 << 4 |
                                         m_.p17 << 5 | m_.p18 << 6 | m_.p19 << 7);
    }
    bool bus_driven() const { return m_.p12_oe != 0; }
    int p2_low() const { return m_.p21 | m_.p22 << 1 | m_.p23 << 2 | m_.p24 << 3; }
    int p1() const
    {
        return m_.p27 | m_.p28 << 1 | m_.p29 << 2 | m_.p30 << 3 | m_.p31 << 4 | m_.p32 << 5 | m_.p33 << 6 | m_.p34 << 7;
    }
    int p2() const { return p2_low() | m_.p35 << 4 | m_.p36 << 5 | m_.p37 << 6 | m_.p38 << 7; }

    // one master tick
    void tick()
    {
        if (ticks % 13 == 0) m_.p2 = !m_.p2;
        m_.p6 = static_cast<CData>(int_n);
        m_.p39 = static_cast<CData>(t1);
        // the latch: open while ALE is high (К589ИР12 with DS2 on ALE)
        if (m_.p11 && bus_driven()) latched = bus_out();
        // the EPROM: its byte while PSEN is low
        const std::uint8_t byte = rom[static_cast<std::size_t>((p2_low() << 8) | latched)];
        const bool reading = !m_.p9;
        std::uint8_t in = reading ? byte : (bus_driven() ? bus_out() : 0xFF);
        set_bus_in(in);
        set_ports_in();
        m_.clk = 0;
        m_.eval();
        m_.clk = 1;
        m_.eval();
        ++ticks;
    }

    void run(std::uint64_t n)
    {
        for (std::uint64_t i = 0; i < n; ++i) tick();
    }

    // machine cycles: 30 half-periods of 13 ticks
    void cycles(int n) { run(static_cast<std::uint64_t>(n) * 30 * 13); }

    std::uint8_t ram(int at) { return r()->km1816ve48__DOT__ram[static_cast<std::size_t>(at)]; }
    int a() { return r()->km1816ve48__DOT__a; }
    int pc() { return r()->km1816ve48__DOT__pc; }
    int cy() { return r()->km1816ve48__DOT__cy; }

private:
    void set_bus_in(std::uint8_t v)
    {
        m_.p12_in = v & 1; m_.p13_in = v >> 1 & 1; m_.p14_in = v >> 2 & 1; m_.p15_in = v >> 3 & 1;
        m_.p16_in = v >> 4 & 1; m_.p17_in = v >> 5 & 1; m_.p18_in = v >> 6 & 1; m_.p19_in = v >> 7 & 1;
    }
    // the ports read back what they drive; P20..P23 read 0xA when let go
    void set_ports_in()
    {
        m_.p27_in = m_.p27; m_.p28_in = m_.p28; m_.p29_in = m_.p29; m_.p30_in = m_.p30;
        m_.p31_in = m_.p31; m_.p32_in = m_.p32; m_.p33_in = m_.p33; m_.p34_in = m_.p34;
        const bool off = !m_.p21_oe;
        m_.p21_in = off ? 0 : m_.p21; m_.p22_in = off ? 1 : m_.p22;
        m_.p23_in = off ? 0 : m_.p23; m_.p24_in = off ? 1 : m_.p24;
        m_.p35_in = m_.p35; m_.p36_in = m_.p36; m_.p37_in = m_.p37; m_.p38_in = m_.p38;
    }

    std::unique_ptr<Vkm1816ve48> model_ = std::make_unique<Vkm1816ve48>();
    Vkm1816ve48& m_ = *model_;
};

std::unique_ptr<Bench> with(const std::vector<std::uint8_t>& program, std::size_t at = 0)
{
    auto b = std::make_unique<Bench>();
    for (std::size_t i = 0; i < program.size(); ++i) b->rom[at + i] = program[i];
    b->run(400);
    b->reset_release();
    return b;
}

}  // namespace

TEST_CASE("КМ1816ВЕ48: the fetch's pins in half-periods, as the data sheet's bus timing")
{
    auto b = with({});                                  // NOPs
    b->cycles(3);
    // half-period by half-period over a cycle: the levels at each
    std::vector<int> ale, psen, addr;
    for (int i = 0; i < 60; ++i) {
        ale.push_back(b->m().p11);
        psen.push_back(b->m().p9);
        addr.push_back(b->bus_driven() ? 1 : 0);
        b->run(13);
    }
    auto run_of = [](const std::vector<int>& v, int level) {
        int best = 0, n = 0;
        for (int x : v) { n = x == level ? n + 1 : 0; if (n > best) best = n; }
        return best;
    };
    CHECK(run_of(ale, 1) == 7);                          // tLL 7/30 tCY
    CHECK(run_of(psen, 0) == 12);                        // tCC 2/5 tCY
    CHECK(run_of(addr, 1) == 5);                         // tAL 3 + tLA 2
    // the order: the address on BUS while ALE is still high, then off, then PSEN
    for (std::size_t i = 1; i < 60; ++i)
        if (ale[i - 1] && !ale[i]) {                     // ALE falls
            CHECK(addr[i - 3]);
            CHECK(addr[i + 1]);
            CHECK(!addr[i + 2]);
            CHECK(psen[i + 2]);
            CHECK(!psen[i + 3]);                         // tAFC: PSEN a half-period after
        }
}

TEST_CASE("КМ1816ВЕ48: NOPs fetched one after another from 0")
{
    auto b = with({});
    b->cycles(10);
    CHECK(b->pc() >= 8);
    CHECK(b->pc() <= 11);
}

TEST_CASE("КМ1816ВЕ48: arithmetic and its flags (ADD, ADDC, DA as the manual's example)")
{
    // MOV A,#9BH; DA A (p. 4-14: -> 01, C set); MOV R2,A; MOV A,#7FH; ADD A,#1;
    // MOV R3,A; CLR C; CPL C; MOV A,#0FFH; ADDC A,#0 -> 00 C; MOV R4,A; JMP $
    auto b = with({0x23, 0x9B, 0x57, 0xAA, 0x23, 0x7F, 0x03, 0x01, 0xAB, 0x97, 0xA7, 0x23, 0xFF,
                   0x13, 0x00, 0xAC, 0x04, 0x10});
    b->cycles(40);
    CHECK(b->ram(2) == 0x01);
    CHECK(b->ram(3) == 0x80);
    CHECK(b->ram(4) == 0x00);
    CHECK(b->cy() == 1);
}

TEST_CASE("КМ1816ВЕ48: jumps, a loop by DJNZ, CALL and RET, conditional jumps")
{
    // 00 MOV R7,#5; 02 MOV R6,#0; 04 INC R6; 05 DJNZ R7,04; 07 CALL 20; 09 MOV R5,A;
    // 0A CLR A; 0B JZ 10; 0D MOV R4,#1; 0F NOP; 10 JB0 14 (A=0: falls); 12 MOV R3,#2;
    // 14 JMP 14; 20 MOV A,#42; 22 RET
    auto b = with({0xBF, 0x05, 0xBE, 0x00, 0x1E, 0xEF, 0x04, 0x14, 0x20, 0xAD, 0x27, 0xC6, 0x10,
                   0xBC, 0x01, 0x00, 0x12, 0x14, 0xBB, 0x02, 0x04, 0x14});
    b->rom[0x20] = 0x23; b->rom[0x21] = 0x42; b->rom[0x22] = 0x83;
    b->cycles(60);
    CHECK(b->ram(6) == 5);
    CHECK(b->ram(7) == 0);
    CHECK(b->ram(5) == 0x42);
    CHECK(b->ram(4) == 0);                               // JZ taken past it
    CHECK(b->ram(3) == 2);                               // JB0 not taken
    CHECK(b->pc() >= 0x14);
    CHECK(b->pc() <= 0x16);
}

TEST_CASE("КМ1816ВЕ48: data memory - @R, XCH, XCHD, the register banks; MOVP3")
{
    // MOV R0,#30H; MOV @R0,#5AH; MOV A,#0A5H; XCHD A,@R0; MOV R1,A; SEL RB1; MOV R0,#7;
    // SEL RB0; MOV A,#2; MOVP3 A,@A; MOV R2,A; JMP $
    auto b = with({0xB8, 0x30, 0xB0, 0x5A, 0x23, 0xA5, 0x30, 0xA9, 0xD5, 0xB8, 0x07, 0xC5, 0x23,
                   0x02, 0xE3, 0xAA, 0x04, 0x10});
    b->rom[0x302] = 0xC3;
    b->cycles(40);
    CHECK(b->ram(0x30) == 0x55);
    CHECK(b->ram(1) == 0xAA);
    CHECK(b->ram(24) == 7);
    CHECK(b->ram(2) == 0xC3);
}

TEST_CASE("КМ1816ВЕ48: ports 1 and 2 - OUTL, ORL, ANL")
{
    // MOV A,#0F0H; OUTL P1,A; ORL P1,#0FH; ANL P1,#3CH; MOV A,#5FH; OUTL P2,A; ANL P2,#0F0H; JMP $
    auto b = with({0x23, 0xF0, 0x39, 0x89, 0x0F, 0x99, 0x3C, 0x23, 0x5F, 0x3A, 0x9A, 0xF0, 0x04,
                   0x0C});
    b->cycles(30);
    CHECK(b->p1() == 0x3C);
    CHECK((b->p2() & 0xF0) == 0x50);
    CHECK((b->r()->km1816ve48__DOT__port2 & 0xFF) == 0x50);
}

TEST_CASE("КМ1816ВЕ48: MOVD to the 8243 - the order as PROG falls, the data as it rises")
{
    // MOV A,#9; MOVD P5,A; JMP $
    auto b = with({0x23, 0x09, 0x3D, 0x04, 0x03});
    int falls = 0, order = -1, data = -1;
    bool was = true;
    for (int i = 0; i < 20 * 30 * 13; ++i) {
        const bool prog = b->m().p25 != 0;
        if (was && !prog) { ++falls; order = b->p2_low(); }
        if (!was && prog) data = b->p2_low();
        was = prog;
        b->tick();
    }
    CHECK(falls == 1);
    CHECK(order == 0x5);                                 // write (01) to port 5 (01)
    CHECK(data == 0x9);
}

TEST_CASE("КМ1816ВЕ48: MOVD from the 8243 - P20..P23 let go, read as PROG rises")
{
    // MOVD A,P4; MOV R2,A; JMP $   (the bench's P20..P23 read 0xA when let go)
    auto b = with({0x0C, 0xAA, 0x04, 0x02});
    b->cycles(20);
    CHECK(b->ram(2) == 0x0A);
}

TEST_CASE("КМ1816ВЕ48: the external interrupt - INT low calls 3, RETR comes back")
{
    // 00 JMP 10; 03 MOV R2,#1; 05 RETR; 10 EN I; 11 INC R3; 12 JMP 11
    auto b = with({0x04, 0x10, 0x00, 0xBA, 0x01, 0x93});
    b->rom[0x10] = 0x05; b->rom[0x11] = 0x1B; b->rom[0x12] = 0x04; b->rom[0x13] = 0x11;
    b->cycles(30);
    CHECK(b->ram(2) == 0);
    b->int_n = 0;
    b->cycles(6);
    b->int_n = 1;
    b->cycles(30);
    CHECK(b->ram(2) == 1);
    CHECK(b->r()->km1816ve48__DOT__in_int == 0);
    CHECK(b->ram(3) > 10);
}

TEST_CASE("КМ1816ВЕ48: the timer - 32 cycles a count, its overflow calls 7")
{
    // 00 JMP 10; 07 INC R2; 08 RETR; 10 MOV A,#0FEH; MOV T,A; EN TCNTI; STRT T; 15 JMP 15
    auto b = with({0x04, 0x10});
    b->rom[0x07] = 0x1A; b->rom[0x08] = 0x93;
    const std::vector<std::uint8_t> main = {0x23, 0xFE, 0x62, 0x25, 0x55, 0x04, 0x15};
    for (std::size_t i = 0; i < main.size(); ++i) b->rom[0x10 + i] = main[i];
    b->cycles(10 + 32);
    CHECK(b->ram(2) == 0);
    CHECK(b->r()->km1816ve48__DOT__t == 0xFF);
    b->cycles(40);
    CHECK(b->ram(2) == 1);
}
