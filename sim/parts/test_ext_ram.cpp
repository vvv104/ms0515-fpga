// ext_ram (rtl/parts/ext_ram.sv): the module's RAM in DDR3 at the pins of
// its sixteen КР565РУ7 - against the chips' own models on the same strobes
// (sim/parts/ext_ram_check.sv), cycles as the module makes them: the
// address on MA00..MA17 first, then a row strobe, the multiplexers' turn to
// the column, a column strobe of one bank or both, early writes, late
// writes, reads, and rows strobed alone between them.  DDR3 is the test's
// model (sim/board/ddram.h), slow and held up at random.
#include <doctest/doctest.h>

#include "Vext_ram_check.h"
#include "../board/ddram.h"

#include <cstdint>
#include <memory>
#include <random>

namespace {

struct Bench {
    std::unique_ptr<Vext_ram_check> m = std::make_unique<Vext_ram_check>();
    Ddram ddr;
    std::uint64_t t = 0;
    std::uint64_t differ = 0;           // ticks the bridge's DO differs from the chips'
    std::uint64_t read_ticks = 0;       // ticks a DO was on

    explicit Bench(int min_ticks, int max_ticks, int busy_one_in) : ddr(min_ticks, max_ticks, busy_one_in)
    {
        m->ras_n = m->cas0_n = m->cas1_n = m->we_n = 1;
        m->reset = 1;
        run(4);
        m->reset = 0;
    }

    void run(int ticks)
    {
        for (int i = 0; i < ticks; ++i, ++t) {
            ddr.present(*m, t);
            m->clk = 0;
            m->eval();
            ddr.take(*m, t);
            m->clk = 1;
            m->eval();
            const unsigned on = m->chip_q_oe;
            if (m->q_oe != on || ((m->q ^ m->chip_q) & on) != 0)
                ++differ;
            if (on != 0)
                ++read_ticks;
        }
    }

    // one cycle at the address ma, as the module makes it: the row and the
    // column out of MA by D15, D17, D18; banks 1, 2 or 3 (both)
    void cycle(std::uint32_t ma, int banks, bool write, bool late, std::uint8_t data)
    {
        const unsigned row = ((ma >> 16 & 1) << 8) | (ma & 0xff);
        const unsigned col = ((ma >> 17 & 1) << 8) | (ma >> 8 & 0xff);
        m->ra = row;
        run(3);
        m->ras_n = 0;
        run(4);
        m->ra = col;
        m->di = data;
        if (write && !late)
            m->we_n = 0;
        run(3);
        if (banks & 1) m->cas0_n = 0;
        if (banks & 2) m->cas1_n = 0;
        run(8);
        if (write && late)
            m->we_n = 0;
        run(12);
        m->cas0_n = m->cas1_n = 1;
        m->we_n = 1;
        run(2);
        m->ras_n = 1;
        run(10);
    }

    void refresh(unsigned row)
    {
        m->ra = row;
        run(3);
        m->ras_n = 0;
        run(20);
        m->ras_n = 1;
        run(10);
    }
};

// cells at random addresses written and read back, with the address set
// `lead` ticks before each cycle; with `resets`, now and then another
// address first and the core's reset a few ticks after - its word on its
// way, or its request still held - so the next word is asked for before
// that one comes
void exercise(Bench& b, int cycles, int lead, std::uint32_t seed, bool resets = false)
{
    std::mt19937 rng(seed);
    std::uniform_int_distribution<std::uint32_t> any(0, (1u << 18) - 1);
    std::uint32_t used[64] = {};
    for (int i = 0; i < cycles; ++i) {
        const bool write = i < 64 || rng() % 2 == 0;
        const std::uint32_t ma = i < 64 || write ? any(rng) : used[rng() % 64];
        if (write) used[i % 64] = ma;
        if (resets && rng() % 4 == 0) {
            b.m->ma = any(rng) >> 2;
            b.run(2 + static_cast<int>(rng() % 4));
            b.m->reset = 1;
            b.run(1 + static_cast<int>(rng() % 3));
            b.m->reset = 0;
        }
        b.m->ma = ma >> 2;
        b.run(lead);
        b.cycle(ma, 1 + static_cast<int>(rng() % 3), write, rng() % 2 == 0, static_cast<std::uint8_t>(rng()));
        if (rng() % 4 == 0)
            b.refresh(any(rng) & 0x1ff);
    }
}

}  // namespace

TEST_CASE("ext_ram: DO as the sixteen chips', DDR3 slow and held up, the address set a microsecond ahead")
{
    Bench b(12, 48, 4);                 // 100-400 ns, busy a tick in four
    exercise(b, 2000, 120, 7);
    CHECK(b.read_ticks > 1000);
    CHECK(b.differ == 0);
    CHECK(b.m->faults == 0);
    CHECK(b.ddr.writes() > 900);
}

// The core's reset (a power cycle for the board's logic) may come with a
// word on its way: that word is let go, not taken for the next one asked.
TEST_CASE("ext_ram: a reset with a word on its way loses nothing")
{
    Bench b(12, 48, 4);
    exercise(b, 2000, 120, 11, true);
    CHECK(b.read_ticks > 1000);
    CHECK(b.differ == 0);
    CHECK(b.m->faults == 0);
}

TEST_CASE("ext_ram: an address set too late for DDR3 is counted, not passed over")
{
    Bench b(60, 60, 0);                 // 500 ns, longer than the cycle's row-to-column time
    exercise(b, 200, 0, 9);
    CHECK(b.m->faults > 0);
}
