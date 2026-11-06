// dram_watch on the board's nets set by hand (the model is not run): a
// write is taken by its strobes' edges, a read checked against it - and no
// write is taken from strobes that stand low at the watch's first tick,
// which no КР565РУ5 takes either (rtl/chips/kr565ru5.sv).
#include <doctest/doctest.h>

#include "dram_watch.h"

#include "Vns4.h"

#include <cstdint>
#include <memory>

namespace {

struct Nets {
    std::unique_ptr<Vns4> m = std::make_unique<Vns4>();
    DramWatch watch;
    std::uint64_t t = 0;

    Nets() { strobes(true, true, true); }

    void strobes(bool ras, bool cas, bool we)
    {
        m->n_VAS_L = ras;
        m->n_VAK_L = cas;
        m->n_ZP_ML_L = we;
        m->n_ZP_ST_L = we;
    }
    void address(std::uint8_t a)
    {
        CData* const bit[8] = {&m->n_A0_H, &m->n_A1_H, &m->n_A2_H, &m->n_A3_H,
                               &m->n_A4_H, &m->n_A5_H, &m->n_A6_H, &m->n_A7_H};
        for (int i = 0; i < 8; ++i) *bit[i] = (a >> i) & 1u;
    }
    void in(std::uint16_t d)
    {
        CData* const bit[16] = {&m->n_MK00_H, &m->n_MK01_H, &m->n_MK02_H, &m->n_MK03_H, &m->n_MK04_H, &m->n_MK05_H,
                                &m->n_MK06_H, &m->n_MK07_H, &m->n_MK08_H, &m->n_MK09_H, &m->n_MK10_H, &m->n_MK11_H,
                                &m->n_MK12_H, &m->n_MK13_H, &m->n_MK14_H, &m->n_MK15_H};
        for (int i = 0; i < 16; ++i) *bit[i] = (d >> i) & 1u;
    }
    void out(std::uint16_t d)
    {
        CData* const bit[16] = {&m->n_D00_H, &m->n_D01_H, &m->n_D02_H, &m->n_D03_H, &m->n_D04_H, &m->n_D05_H,
                                &m->n_D06_H, &m->n_D07_H, &m->n_D08_H, &m->n_D09_H, &m->n_D10_H, &m->n_D11_H,
                                &m->n_D12_H, &m->n_D13_H, &m->n_D14_H, &m->n_D15_H};
        for (int i = 0; i < 16; ++i) *bit[i] = (d >> i) & 1u;
    }
    void tick(int n = 1)
    {
        for (int i = 0; i < n; ++i) watch.see(*m, t++);
    }

    // a cycle at a cell: the row, -RAS, the column, -CAS; an early write of `d`, or a read that gives `d`
    void cycle(std::uint8_t row, std::uint8_t column, bool write, std::uint16_t d)
    {
        address(row);
        tick(2);
        strobes(false, true, !write);
        tick(2);
        address(column);
        write ? in(d) : out(d);
        strobes(false, false, !write);
        tick(4);
        strobes(true, true, true);
        tick(2);
    }
};

}  // namespace

TEST_CASE("dram_watch: a read is checked against what was written there")
{
    Nets n;
    n.tick(4);
    n.cycle(0x12, 0x34, true, 0xA55A);
    CHECK(n.watch.stats().writes == 1);
    CHECK(n.watch.known(0x1234) == 3);
    CHECK(n.watch.word(0x1234) == 0xA55A);
    n.cycle(0x12, 0x34, false, 0xA55A);
    CHECK(n.watch.stats().checked == 1);
    CHECK(n.watch.stats().differ == 0);
    const std::uint64_t at = n.t;
    n.cycle(0x12, 0x34, false, 0xA55B);
    CHECK(n.watch.stats().differ == 1);
    CHECK(n.watch.stats().differ_video == 0);
    CHECK(n.watch.stats().first_differ >= at);
    CHECK(n.watch.stats().first_differ < n.t);
    CHECK(n.watch.stats().last_differ == n.watch.stats().first_differ);
    n.cycle(0x56, 0x78, false, 0x1111);         // a cell never written is not checked
    CHECK(n.watch.stats().checked == 2);
}

// At max from an unseeded power-up the strobes and both -WE stand low at
// tick 0 with 177605 on МК: the watch took it for a write of the cell
// 000:000, and the processor's 900 reads of that cell before ROM-B's test
// writes it (to 0.207 s) all came out "not as written".
TEST_CASE("dram_watch: strobes low at the first tick are no write")
{
    Nets n;
    n.address(0);
    n.in(0xFF85);
    n.strobes(false, false, false);
    n.tick(3);
    CHECK(n.watch.stats().writes == 0);
    CHECK(n.watch.known(0) == 0);
    n.strobes(true, true, true);
    n.tick(3);
    n.cycle(0, 0, false, 0x0000);
    CHECK(n.watch.stats().checked == 0);
    CHECK(n.watch.stats().differ == 0);
    n.cycle(0, 0, true, 0x0000);                // the first write seen by its edges
    n.cycle(0, 0, false, 0x0000);
    CHECK(n.watch.stats().checked == 1);
    CHECK(n.watch.stats().differ == 0);
}
