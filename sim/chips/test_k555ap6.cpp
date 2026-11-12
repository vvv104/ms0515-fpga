// К555АП6 (74LS245): eight transceivers - DIR high drives B from A, low A
// from B, G high isolates both sides (TI SN74LS245 SDLS146B table 1).
#include "chip.h"

#include "Vk555ap6.h"

#include <random>

namespace {

using V = Vk555ap6;

void set_a(V& m, unsigned v)
{
    m.p2_in = v & 1u;         m.p3_in = (v >> 1) & 1u; m.p4_in = (v >> 2) & 1u; m.p5_in = (v >> 3) & 1u;
    m.p6_in = (v >> 4) & 1u;  m.p7_in = (v >> 5) & 1u; m.p8_in = (v >> 6) & 1u; m.p9_in = (v >> 7) & 1u;
}

void set_b(V& m, unsigned v)
{
    m.p18_in = v & 1u;        m.p17_in = (v >> 1) & 1u; m.p16_in = (v >> 2) & 1u; m.p15_in = (v >> 3) & 1u;
    m.p14_in = (v >> 4) & 1u; m.p13_in = (v >> 5) & 1u; m.p12_in = (v >> 6) & 1u; m.p11_in = (v >> 7) & 1u;
}

unsigned a_out(V& m)
{
    return m.p2 | (m.p3 << 1) | (m.p4 << 2) | (m.p5 << 3) | (m.p6 << 4) | (m.p7 << 5) | (m.p8 << 6) | (m.p9 << 7);
}

unsigned b_out(V& m)
{
    return m.p18 | (m.p17 << 1) | (m.p16 << 2) | (m.p15 << 3) | (m.p14 << 4) | (m.p13 << 5) | (m.p12 << 6) |
           (m.p11 << 7);
}

unsigned a_oe(V& m)
{
    return m.p2_oe & m.p3_oe & m.p4_oe & m.p5_oe & m.p6_oe & m.p7_oe & m.p8_oe & m.p9_oe;
}

unsigned a_any(V& m)
{
    return m.p2_oe | m.p3_oe | m.p4_oe | m.p5_oe | m.p6_oe | m.p7_oe | m.p8_oe | m.p9_oe;
}

unsigned b_oe(V& m)
{
    return m.p18_oe & m.p17_oe & m.p16_oe & m.p15_oe & m.p14_oe & m.p13_oe & m.p12_oe & m.p11_oe;
}

unsigned b_any(V& m)
{
    return m.p18_oe | m.p17_oe | m.p16_oe | m.p15_oe | m.p14_oe | m.p13_oe | m.p12_oe | m.p11_oe;
}

}  // namespace

TEST_CASE("К555АП6: A to B, B to A, isolated")
{
    V m;
    std::mt19937 rng(245);
    for (int i = 0; i < 200; ++i) {
        const unsigned a = rng() & 0xFFu, b = rng() & 0xFFu;
        set_a(m, a);
        set_b(m, b);
        m.p19 = 0;
        m.p1 = 1;                                   // A to B
        m.eval();
        CHECK(b_oe(m) == 1u);
        CHECK(a_any(m) == 0u);
        CHECK(b_out(m) == a);
        m.p1 = 0;                                   // B to A
        m.eval();
        CHECK(a_oe(m) == 1u);
        CHECK(b_any(m) == 0u);
        CHECK(a_out(m) == b);
        m.p19 = 1;                                  // isolated, either way
        m.p1 = rng() & 1u;
        m.eval();
        CHECK(a_any(m) == 0u);
        CHECK(b_any(m) == 0u);
    }
}
