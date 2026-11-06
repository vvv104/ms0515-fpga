// КР580ВВ51А (8251A) through its pins, asynchronous: the reset sequence,
// a transmitted frame bit by bit, reception, the errors, break detect
// (docs/vv51.md).  CLK is a period of 8 master ticks, TxC = RxC 40.
#include "chip.h"

#include "Vkr580vv51.h"

#include <cstdint>
#include <memory>
#include <vector>

namespace {

using V = Vkr580vv51;
constexpr int kClkHalf = 4;
constexpr int kBaudHalf = 20;                   // a TxC/RxC half period

class Usart {
public:
    Usart()
    {
        m().p11 = 1; m().p13 = 1; m().p10 = 1;  // -CS -RD -WR
        m().p17 = 0; m().p22 = 0;               // -CTS -DSR asserted
        m().p3 = 1;                             // RxD marking
        m().p21 = 1;
        run(40);
        m().p21 = 0;
        run(8);
    }

    V& m() { return *m_; }

    // master ticks, CLK and the baud clock running
    void run(int ticks)
    {
        for (int i = 0; i < ticks; ++i) {
            m().p20 = (t_ / kClkHalf) & 1;
            const bool baud = ((t_ / kBaudHalf) & 1) == 0;
            m().p9 = baud;
            m().p25 = baud;
            if (baud_was_ && !baud) {           // a TxC fall: TxD as it stood
                txd_.push_back(m().p19 ? 1 : 0);
            }
            baud_was_ = baud;
            m().clk = 0;
            m().eval();
            m().clk = 1;
            m().eval();
            ++t_;
        }
    }

    void write(bool control, std::uint8_t v)
    {
        m().p12 = control;
        CData* d[8] = {&m().p27_in, &m().p28_in, &m().p1_in, &m().p2_in, &m().p5_in, &m().p6_in,
                       &m().p7_in, &m().p8_in};
        for (int i = 0; i < 8; ++i) *d[i] = (v >> i) & 1u;
        m().p11 = 0; m().p10 = 0;
        run(4);
        m().p10 = 1;
        run(2);
        m().p11 = 1;
        run(30);
    }

    std::uint8_t read(bool control)
    {
        m().p12 = control;
        m().p11 = 0; m().p13 = 0;
        run(3);
        const CData* d[8] = {&m().p27, &m().p28, &m().p1, &m().p2, &m().p5, &m().p6, &m().p7, &m().p8};
        std::uint8_t v = 0;
        for (int i = 0; i < 8; ++i) v = static_cast<std::uint8_t>(v | (*d[i] & 1u) << i);
        REQUIRE(m().p27_oe);
        m().p13 = 1; m().p11 = 1;
        run(3);
        return v;
    }

    // a frame on RxD at x16: start, the bits LSB first, the stop level
    void send(const std::vector<int>& bits)
    {
        for (int b : bits) {
            m().p3 = b;
            run(16 * 2 * kBaudHalf);
        }
        m().p3 = 1;
    }

    static std::vector<int> frame(std::uint8_t v, int n, int parity, int stop)
    {
        std::vector<int> b{0};
        for (int i = 0; i < n; ++i) b.push_back((v >> i) & 1);
        if (parity >= 0) b.push_back(parity);
        b.push_back(stop);
        return b;
    }

    std::vector<int> txd_;                      // TxD at each TxC fall

private:
    std::unique_ptr<V> m_ = std::make_unique<V>();
    std::uint64_t t_ = 0;
    bool baud_was_ = true;
};

// The datasheet's safe initialisation: three zeros, the internal reset,
// the mode, the command.
void init(Usart& u, std::uint8_t mode, std::uint8_t command)
{
    u.write(true, 0x00);
    u.write(true, 0x00);
    u.write(true, 0x00);
    u.write(true, 0x40);
    u.write(true, mode);
    u.write(true, command);
}

// The characters on TxD (its level at each TxC fall) at x16, 8 bits, no
// parity: a start bit, the bits taken at each one's first TxC period.
std::vector<int> sent(const std::vector<int>& t)
{
    std::vector<int> out;
    std::size_t s = 0;
    while (s < t.size()) {
        if (t[s] == 1) { ++s; continue; }
        if (s + 9 * 16 > t.size()) break;
        int v = 0;
        for (std::size_t b = 0; b < 8; ++b) v |= t[s + 16 * (b + 1)] << b;
        out.push_back(v);
        s += 10 * 16;                           // past the first stop bit
    }
    return out;
}

constexpr int kFrame = 12 * 16 * 2 * kBaudHalf; // a character with two stop bits, and to spare

}  // namespace

TEST_CASE("КР580ВВ51: the reset sequence, then the mode and the command")
{
    Usart u;
    init(u, 0xCE, 0x15);                        // 2 stop, 8 bits, x16; TxEN RxE ER
    CHECK(u.read(true) == 0x85);                // DSR, TxEMPTY, TxRDY
    CHECK(u.m().p15);                           // TxRDY pin
    CHECK(u.m().p24);                           // -DTR high: the command left DTR off
    u.write(true, 0x17);                        // ... and now on
    CHECK(!u.m().p24);
}

TEST_CASE("КР580ВВ51: a character leaves at x16: start, LSB first, two stop bits")
{
    Usart u;
    init(u, 0xCE, 0x05);
    u.txd_.clear();
    u.write(false, 0xA5);
    CHECK(!u.m().p18);                          // TxEMPTY low while sending
    u.run(13 * 16 * 2 * kBaudHalf);
    const std::vector<int>& t = u.txd_;
    std::size_t s = 0;
    while (s < t.size() && t[s] == 1) ++s;
    REQUIRE(s + 11 * 16 <= t.size());
    std::vector<int> bits;
    for (int b = 0; b < 11; ++b) {
        for (int k = 0; k < 16; ++k)            // each bit exactly 16 TxC periods
            CHECK(t[s + static_cast<std::size_t>(16 * b + k)] == t[s + static_cast<std::size_t>(16 * b)]);
        bits.push_back(t[s + static_cast<std::size_t>(16 * b)]);
    }
    CHECK(bits == std::vector<int>{0, 1, 0, 1, 0, 0, 1, 0, 1, 1, 1});
    CHECK(u.m().p18);                           // TxEMPTY back
    CHECK(u.m().p19);                           // marking
}

// The 8251A's own (docs/vv51.md §7, DS p.2-2, p.2-4): a Tx Disable or -CTS
// going off does not stop what was written before it.  The MS 0515's
// programs write the keyboard a command and take TxEN off in the next
// instruction (BIRDS: 231, 231, then the command 064).
TEST_CASE("КР580ВВ51: what was written before TxEN went off is still sent")
{
    Usart u;
    init(u, 0xCE, 0x05);
    u.txd_.clear();
    u.write(false, 0xA5);                       // into the shifter
    u.write(false, 0x3C);                       // waits in the buffer
    u.write(true, 0x04);                        // TxEN off, before the first has left
    CHECK((u.read(true) & 0x01) == 0);          // the buffer still holds the second
    u.run(3 * kFrame);
    CHECK(sent(u.txd_) == std::vector<int>{0xA5, 0x3C});
    CHECK((u.read(true) & 0x05) == 0x05);       // TxRDY and TxEMPTY: nothing left
    CHECK(u.m().p19);                           // marking
}

// What the datasheet leaves unsaid, taken from the machine's own software:
// under Omega ROM-B leaves the keyboard USART's command at 006 (TxEN off),
// and BIRDS and Saboteur 2 begin by waiting for TxRDY, writing the keyboard
// 231, waiting for TxRDY again and writing 231 once more.  So with TxEN
// off a character leaves the buffer - for the shift register, where it
// waits for TxEN - and TxRDY comes back; TxEMPTY stays high (DS p.2-4).
TEST_CASE("КР580ВВ51: a character written with TxEN off leaves the buffer and waits in the shifter")
{
    Usart u;
    init(u, 0x4E, 0x06);                        // ROM-B's: 1 stop bit, 8 bits, x16; DTR, RxE - no TxEN
    u.txd_.clear();
    CHECK((u.read(true) & 0x01) != 0);
    u.write(false, 0x99);
    u.run(2 * kFrame);
    CHECK(sent(u.txd_).empty());                // nothing on the line
    CHECK((u.read(true) & 0x05) == 0x05);       // TxRDY back, TxEMPTY still high
    u.write(false, 0x66);                       // the second fills the buffer
    u.run(2 * kFrame);
    CHECK((u.read(true) & 0x01) == 0);
    CHECK(sent(u.txd_).empty());
    CHECK(u.m().p19);
    u.write(true, 0x07);                        // TxEN: both leave, in their order
    u.run(3 * kFrame);
    CHECK(sent(u.txd_) == std::vector<int>{0x99, 0x66});
    CHECK((u.read(true) & 0x05) == 0x05);
}

TEST_CASE("КР580ВВ51: what was written before -CTS went off is still sent")
{
    Usart u;
    init(u, 0xCE, 0x05);
    u.txd_.clear();
    u.write(false, 0xA5);
    u.write(false, 0x3C);
    u.m().p17 = 1;                              // -CTS off
    u.run(3 * kFrame);
    CHECK(sent(u.txd_) == std::vector<int>{0xA5, 0x3C});
    u.txd_.clear();
    u.write(false, 0x55);                       // written with -CTS off: it waits
    u.run(2 * kFrame);
    CHECK(sent(u.txd_).empty());
    u.m().p17 = 0;
    u.run(2 * kFrame);
    CHECK(sent(u.txd_) == std::vector<int>{0x55});
}

TEST_CASE("КР580ВВ51: a character arrives; RxRDY until it is read; overrun")
{
    Usart u;
    init(u, 0x4E, 0x14);                        // 1 stop, 8 bits, x16; RxE ER
    u.run(200);
    u.send(Usart::frame(0x3C, 8, -1, 1));
    u.run(400);
    CHECK(u.m().p14);
    CHECK(u.read(false) == 0x3C);
    CHECK(!u.m().p14);
    u.send(Usart::frame(0x11, 8, -1, 1));
    u.run(400);
    u.send(Usart::frame(0x22, 8, -1, 1));
    u.run(400);
    CHECK((u.read(true) & 0x10) != 0);          // OE
    CHECK(u.read(false) == 0x22);
    u.write(true, 0x14);                        // ER clears it
    CHECK((u.read(true) & 0x38) == 0);
}

TEST_CASE("КР580ВВ51: parity and framing errors, break detect")
{
    Usart u;
    init(u, 0x7A, 0x14);                        // 1 stop, even parity, 7 bits, x16
    u.run(200);
    u.send(Usart::frame(0x41, 7, 1, 1));        // 0x41 has two ones: even parity is 0, 1 is wrong
    u.run(400);
    CHECK((u.read(true) & 0x08) != 0);          // PE
    CHECK(u.read(false) == 0x41);
    u.write(true, 0x14);
    u.send(Usart::frame(0x41, 7, 0, 0));        // no stop bit
    u.run(400);
    CHECK((u.read(true) & 0x28) == 0x20);       // FE, not PE
    u.read(false);
    u.send(Usart::frame(0x00, 7, 0, 0));
    u.send(Usart::frame(0x00, 7, 0, 0));
    CHECK(u.m().p16);                           // break detected
    u.m().p3 = 1;
    u.run(16 * 2 * kBaudHalf * 2);
    CHECK(!u.m().p16);                          // gone with the line high
}
