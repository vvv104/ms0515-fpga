// КР580ВИ53 (8253) through its pins: the bus, the six modes as the Intel
// datasheet's Fig. 7 draws them, BCD, the counter latch (docs/vi53.md).
#include "chip.h"

#include "Vkr580vi53.h"

#include <cstdint>
#include <memory>
#include <vector>

namespace {

using V = Vkr580vi53;
constexpr int kHalf = 4;                        // master ticks a CLK half-period

class Timer {
public:
    Timer()
    {
        m().p21 = 1;                            // -CS
        m().p22 = 1;                            // -RD
        m().p23 = 1;                            // -WR
        for (int c = 0; c < 3; ++c) gate(c, true);
        clk_ = false;
        set_clocks();
        idle(4);
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

    void address(int a)
    {
        m().p19 = a & 1;
        m().p20 = (a >> 1) & 1;
    }

    void write(int a, std::uint8_t v)
    {
        address(a);
        CData* d[8] = {&m().p8_in, &m().p7_in, &m().p6_in, &m().p5_in, &m().p4_in, &m().p3_in,
                       &m().p2_in, &m().p1_in};
        for (int i = 0; i < 8; ++i) *d[i] = (v >> i) & 1u;
        m().p21 = 0;
        m().p23 = 0;
        idle(3);
        m().p23 = 1;
        idle(2);
        m().p21 = 1;
        idle(1);
    }

    std::uint8_t read(int a)
    {
        address(a);
        m().p21 = 0;
        m().p22 = 0;
        idle(3);
        const CData* d[8] = {&m().p8, &m().p7, &m().p6, &m().p5, &m().p4, &m().p3, &m().p2, &m().p1};
        const CData* oe[8] = {&m().p8_oe, &m().p7_oe, &m().p6_oe, &m().p5_oe, &m().p4_oe,
                              &m().p3_oe, &m().p2_oe, &m().p1_oe};
        std::uint8_t v = 0;
        for (int i = 0; i < 8; ++i) {
            REQUIRE(*oe[i]);
            v = static_cast<std::uint8_t>(v | (*d[i] & 1u) << i);
        }
        m().p22 = 1;
        idle(2);
        m().p21 = 1;
        idle(1);
        CHECK(!m().p1_oe);
        return v;
    }

    void gate(int c, bool level)
    {
        CData* g[3] = {&m().p11, &m().p14, &m().p16};
        *g[c] = level;
    }

    bool out(int c)
    {
        const CData* o[3] = {&m().p10, &m().p13, &m().p17};
        return *o[c] != 0;
    }

    // one CLK period on all three inputs: high, then low; OUT seen after the fall
    void clock()
    {
        clk_ = true;
        set_clocks();
        idle(kHalf);
        clk_ = false;
        set_clocks();
        idle(kHalf);
    }

    // OUT of counter c after each of n clocks
    std::vector<int> outs(int c, int n)
    {
        std::vector<int> v;
        for (int i = 0; i < n; ++i) {
            clock();
            v.push_back(out(c) ? 1 : 0);
        }
        return v;
    }

    void program(int c, int mode, unsigned count, bool bcd = false)
    {
        write(3, static_cast<std::uint8_t>(c << 6 | 3 << 4 | mode << 1 | (bcd ? 1 : 0)));
        write(c, static_cast<std::uint8_t>(count & 0xFF));
        write(c, static_cast<std::uint8_t>(count >> 8));
    }

private:
    void set_clocks()
    {
        m().p9 = clk_;
        m().p15 = clk_;
        m().p18 = clk_;
    }

    std::unique_ptr<V> m_ = std::make_unique<V>();
    bool clk_ = false;
};

}  // namespace

TEST_CASE("КР580ВИ53 mode 0: OUT low, high on the (N+1)th clock after the write, GATE holds")
{
    Timer t;
    t.write(3, 0x30);                           // counter 0, LSB+MSB, mode 0
    CHECK(!t.out(0));
    t.write(0, 4);
    t.write(0, 0);
    CHECK(t.outs(0, 6) == std::vector<int>{0, 0, 0, 0, 1, 1});
    t.program(0, 0, 3);
    CHECK(!t.out(0));                           // a new count: low again
    t.clock();                                  // the load clock
    t.gate(0, false);
    CHECK(t.outs(0, 4) == std::vector<int>{0, 0, 0, 0});
    t.gate(0, true);
    CHECK(t.outs(0, 4) == std::vector<int>{0, 0, 1, 1});
}

TEST_CASE("КР580ВИ53 mode 1: a GATE rise starts the pulse, a later rise stretches it")
{
    Timer t;
    t.gate(1, false);
    t.program(1, 1, 3);
    CHECK(t.out(1));
    CHECK(t.outs(1, 3) == std::vector<int>{1, 1, 1});   // no trigger, no pulse
    t.gate(1, true);
    CHECK(t.outs(1, 5) == std::vector<int>{0, 0, 0, 1, 1});
    t.gate(1, false);
    t.clock();
    t.gate(1, true);
    std::vector<int> v = t.outs(1, 2);
    t.gate(1, false);
    t.clock();
    t.gate(1, true);                            // retrigger while low
    for (int x : t.outs(1, 5)) v.push_back(x);
    CHECK(v == std::vector<int>{0, 0, 0, 0, 0, 1, 1});    // low N clocks after the last rise
}

TEST_CASE("КР580ВИ53 mode 2: one clock low every N, GATE low forces OUT high")
{
    Timer t;
    t.program(2, 2, 4);
    CHECK(t.out(2));
    t.clock();                                  // the load clock
    CHECK(t.outs(2, 8) == std::vector<int>{1, 1, 0, 1, 1, 1, 0, 1});
    t.gate(2, false);
    t.idle(2);
    CHECK(t.out(2));
}

TEST_CASE("КР580ВИ53 mode 3: N/2 high and low; odd N high one clock longer")
{
    Timer t;
    t.program(0, 3, 4);
    t.clock();
    CHECK(t.outs(0, 8) == std::vector<int>{1, 0, 0, 1, 1, 0, 0, 1});
    t.program(1, 3, 5);
    t.clock();
    CHECK(t.outs(1, 10) == std::vector<int>{1, 1, 0, 0, 1, 1, 1, 0, 0, 1});
}

TEST_CASE("КР580ВИ53 mode 4: a one-clock strobe after N + 1 clocks")
{
    Timer t;
    t.program(0, 4, 4);
    CHECK(t.out(0));
    CHECK(t.outs(0, 7) == std::vector<int>{1, 1, 1, 1, 0, 1, 1});
}

TEST_CASE("КР580ВИ53 mode 5: a GATE rise starts the strobe")
{
    Timer t;
    t.gate(2, false);
    t.program(2, 5, 3);
    CHECK(t.outs(2, 3) == std::vector<int>{1, 1, 1});
    t.gate(2, true);
    CHECK(t.outs(2, 6) == std::vector<int>{1, 1, 1, 0, 1, 1});
}

TEST_CASE("КР580ВИ53: reads LSB then MSB; the latch holds a count while it runs")
{
    Timer t;
    t.program(0, 2, 0x1234);
    t.clock();                                  // loaded: 1234
    t.clock();                                  // 1233
    CHECK(t.read(0) == 0x33);
    CHECK(t.read(0) == 0x12);
    t.write(3, 0x00);                           // latch counter 0
    t.clock();
    t.clock();
    CHECK(t.read(0) == 0x33);                   // latched at 1233
    CHECK(t.read(0) == 0x12);
    CHECK(t.read(0) == 0x31);                   // released: running at 1231
}

TEST_CASE("КР580ВИ53: D drives a tick after -RD falls and floats a tick after it rises")
{
    // inside tRD (200 ns) and tDF (25..100 ns) of the datasheet: docs/vi53.md
    Timer t;
    t.program(0, 2, 0x1234);
    t.clock();
    t.address(0);
    t.m().p21 = 0;
    t.m().p22 = 0;
    t.m().eval();
    CHECK(!t.m().p1_oe);                        // not at once
    t.idle(1);
    CHECK(t.m().p1_oe);
    t.m().p22 = 1;
    t.m().eval();
    CHECK(t.m().p1_oe);                         // still, for the tick
    t.idle(1);
    CHECK(!t.m().p1_oe);
    t.m().p21 = 1;
    t.idle(1);
}

TEST_CASE("КР580ВИ53: BCD counts in decades, 0 is 10000")
{
    Timer t;
    t.program(1, 2, 0x0010, true);              // BCD 10
    t.clock();
    std::vector<int> v = t.outs(1, 20);
    int lows = 0;
    for (int x : v) lows += x == 0 ? 1 : 0;
    CHECK(lows == 2);                           // a low every 10 clocks
    t.program(0, 2, 0x0000, true);              // 10000 in BCD
    t.clock();
    t.clock();
    CHECK(t.read(0) == 0x99);                   // 9999
    CHECK(t.read(0) == 0x99);
}
