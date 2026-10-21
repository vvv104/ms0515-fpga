// КР580ВВ55 (8255A) through its pins, mode 0: RESET leaves every port an
// input, a mode word clears the latches, the port C bit set/reset word,
// reads of inputs and of outputs (docs/vv55.md).
#include "chip.h"

#include "Vkr580vv55.h"

#include <array>
#include <cstdint>
#include <memory>

namespace {

using V = Vkr580vv55;

class Ppi {
public:
    Ppi()
    {
        m().p6 = 1;                             // -CS
        m().p5 = 1;                             // -RD
        m().p36 = 1;                            // -WR
        m().p35 = 1;                            // RESET
        idle(4);
        m().p35 = 0;
        idle(2);
    }

    V& m() { return *m_; }

    void idle(int ticks)
    {
        for (int i = 0; i < ticks; ++i) {
            m().clk = 0;
            m().eval();
            m().clk = 1;
            m().eval();
        }
    }

    void reset()
    {
        m().p35 = 1;
        idle(3);
        m().p35 = 0;
        idle(2);
    }

    void write(int a, std::uint8_t v)
    {
        m().p9 = a & 1;
        m().p8 = (a >> 1) & 1;
        CData* d[8] = {&m().p34_in, &m().p33_in, &m().p32_in, &m().p31_in, &m().p30_in,
                       &m().p29_in, &m().p28_in, &m().p27_in};
        for (int i = 0; i < 8; ++i) *d[i] = (v >> i) & 1u;
        m().p6 = 0;
        m().p36 = 0;
        idle(3);
        m().p36 = 1;
        idle(2);
        m().p6 = 1;
        idle(1);
    }

    std::uint8_t read(int a)
    {
        m().p9 = a & 1;
        m().p8 = (a >> 1) & 1;
        m().p6 = 0;
        m().p5 = 0;
        idle(2);
        const CData* d[8] = {&m().p34, &m().p33, &m().p32, &m().p31, &m().p30, &m().p29, &m().p28, &m().p27};
        const CData* oe[8] = {&m().p34_oe, &m().p33_oe, &m().p32_oe, &m().p31_oe, &m().p30_oe,
                              &m().p29_oe, &m().p28_oe, &m().p27_oe};
        std::uint8_t v = 0;
        for (int i = 0; i < 8; ++i) {
            REQUIRE(*oe[i]);
            v = static_cast<std::uint8_t>(v | (*d[i] & 1u) << i);
        }
        m().p5 = 1;
        idle(1);
        m().p6 = 1;
        idle(1);
        CHECK(!m().p34_oe);
        return v;
    }

    // a port's pins: [k] is bit k
    std::array<CData*, 8> pins(char port)
    {
        V& x = m();
        if (port == 'A') return {&x.p4, &x.p3, &x.p2, &x.p1, &x.p40, &x.p39, &x.p38, &x.p37};
        if (port == 'B') return {&x.p18, &x.p19, &x.p20, &x.p21, &x.p22, &x.p23, &x.p24, &x.p25};
        return {&x.p14, &x.p15, &x.p16, &x.p17, &x.p13, &x.p12, &x.p11, &x.p10};
    }
    std::array<CData*, 8> enables(char port)
    {
        V& x = m();
        if (port == 'A') return {&x.p4_oe, &x.p3_oe, &x.p2_oe, &x.p1_oe, &x.p40_oe, &x.p39_oe, &x.p38_oe, &x.p37_oe};
        if (port == 'B') return {&x.p18_oe, &x.p19_oe, &x.p20_oe, &x.p21_oe, &x.p22_oe, &x.p23_oe, &x.p24_oe, &x.p25_oe};
        return {&x.p14_oe, &x.p15_oe, &x.p16_oe, &x.p17_oe, &x.p13_oe, &x.p12_oe, &x.p11_oe, &x.p10_oe};
    }
    std::array<CData*, 8> inputs(char port)
    {
        V& x = m();
        if (port == 'A') return {&x.p4_in, &x.p3_in, &x.p2_in, &x.p1_in, &x.p40_in, &x.p39_in, &x.p38_in, &x.p37_in};
        if (port == 'B') return {&x.p18_in, &x.p19_in, &x.p20_in, &x.p21_in, &x.p22_in, &x.p23_in, &x.p24_in, &x.p25_in};
        return {&x.p14_in, &x.p15_in, &x.p16_in, &x.p17_in, &x.p13_in, &x.p12_in, &x.p11_in, &x.p10_in};
    }

    // the bits driven, and their levels
    std::uint8_t driven(char port)
    {
        std::uint8_t v = 0;
        auto e = enables(port);
        for (int i = 0; i < 8; ++i) v = static_cast<std::uint8_t>(v | (*e[static_cast<std::size_t>(i)] & 1u) << i);
        return v;
    }
    std::uint8_t level(char port)
    {
        std::uint8_t v = 0;
        auto p = pins(port);
        for (int i = 0; i < 8; ++i) v = static_cast<std::uint8_t>(v | (*p[static_cast<std::size_t>(i)] & 1u) << i);
        return v;
    }
    void apply(char port, std::uint8_t v)
    {
        auto in = inputs(port);
        for (int i = 0; i < 8; ++i) *in[static_cast<std::size_t>(i)] = (v >> i) & 1u;
        idle(1);
    }

private:
    std::unique_ptr<V> m_ = std::make_unique<V>();
};

}  // namespace

TEST_CASE("КР580ВВ55: after RESET every port is an input")
{
    Ppi p;
    CHECK(p.driven('A') == 0);
    CHECK(p.driven('B') == 0);
    CHECK(p.driven('C') == 0);
    p.apply('A', 0x5A);
    p.apply('B', 0xC3);
    p.apply('C', 0x81);
    CHECK(p.read(0) == 0x5A);
    CHECK(p.read(1) == 0xC3);
    CHECK(p.read(2) == 0x81);
}

TEST_CASE("КР580ВВ55: the НС4 system word 202 - A out, B in, C out; the latches cleared")
{
    Ppi p;
    p.write(3, 0202);
    CHECK(p.driven('A') == 0xFF);
    CHECK(p.driven('B') == 0x00);
    CHECK(p.driven('C') == 0xFF);
    CHECK(p.level('A') == 0);
    CHECK(p.level('C') == 0);
    p.write(0, 0x96);
    p.write(2, 0x3C);
    CHECK(p.level('A') == 0x96);
    CHECK(p.level('C') == 0x3C);
    CHECK(p.read(0) == 0x96);                   // an output reads back its latch
    p.apply('B', 0x11);
    CHECK(p.read(1) == 0x11);
    p.write(3, 0202);                           // a mode word clears again
    CHECK(p.level('A') == 0);
    p.reset();
    CHECK(p.driven('A') == 0);
}

TEST_CASE("КР580ВВ55: the port C bit set/reset word")
{
    Ppi p;
    p.write(3, 0x80);                           // all out
    p.write(3, 0x0F);                           // set PC7
    p.write(3, 0x03);                           // set PC1
    CHECK(p.level('C') == 0x82);
    p.write(3, 0x0E);                           // reset PC7
    CHECK(p.level('C') == 0x02);
    CHECK(p.level('A') == 0);                   // the other ports untouched
}

TEST_CASE("КР580ВВ55: the halves of port C go their own ways")
{
    Ppi p;
    p.write(3, 0x88);                           // A out, C upper in, B out, C lower out
    CHECK(p.driven('C') == 0x0F);
    p.write(2, 0xFF);
    CHECK((p.level('C') & p.driven('C')) == 0x0F);   // only the lower half drives
    p.apply('C', 0xA0);
    CHECK((p.read(2) & 0xF0) == 0xA0);
    CHECK((p.read(2) & 0x0F) == 0x0F);
}
