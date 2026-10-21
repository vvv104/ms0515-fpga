// КР1818ВГ93 (FD1793) through its pins, against a drive: a 300 rpm MFM
// disk built from sector data (the layout of docs/vg93.md §5.1), an ideal
// separator giving RCLK and -RAW READ, the head stepping on STEP/DIRC,
// WD and WG recorded back into the track.  1 us of CLK (1 MHz) is 8
// master ticks here, so a revolution is 1.6 M ticks.
#include "chip.h"

#include "Vkr1818vg93.h"

#include <cstdint>
#include <memory>
#include <vector>

namespace {

using V = Vkr1818vg93;
constexpr int kTicksPerUs = 8;
constexpr int kTrackBytes = 6250;               // 200 ms at 250 kbit/s
constexpr int kTrackCells = kTrackBytes * 16;

std::uint16_t crc_byte(std::uint16_t c, std::uint8_t b)
{
    for (int i = 7; i >= 0; --i) {
        const bool top = ((c >> 15) & 1u) != ((b >> i) & 1u);
        c = static_cast<std::uint16_t>(c << 1);
        if (top) c ^= 0x1021;
    }
    return c;
}

std::uint8_t pattern(int track, int sector, int i)
{
    return static_cast<std::uint8_t>(track * 7 + sector * 13 + i * 3 + (i >> 8));
}

// One side of a disk as MFM cells, track by track.
class Disk {
public:
    Disk() : cells_(80) { for (int t = 0; t < 80; ++t) format(t); }

    std::vector<std::uint8_t>& track(int t) { return cells_[static_cast<std::size_t>(t)]; }

    void format(int t)
    {
        std::vector<std::uint8_t>& c = track(t);
        c.assign(kTrackCells, 0);
        pos_ = 0;
        prev_ = 0;
        put(c, 0x4E, 32);
        for (int s = 1; s <= 10; ++s) {
            put(c, 0x00, 12);
            std::uint16_t crc = 0xFFFF;
            for (int k = 0; k < 3; ++k) { sync(c); crc = crc_byte(crc, 0xA1); }
            const std::uint8_t id[5] = {0xFE, static_cast<std::uint8_t>(t), 0, static_cast<std::uint8_t>(s), 2};
            for (std::uint8_t b : id) { put(c, b, 1); crc = crc_byte(crc, b); }
            put(c, static_cast<std::uint8_t>(crc >> 8), 1);
            put(c, static_cast<std::uint8_t>(crc), 1);
            put(c, 0x4E, 22);
            put(c, 0x00, 12);
            crc = 0xFFFF;
            for (int k = 0; k < 3; ++k) { sync(c); crc = crc_byte(crc, 0xA1); }
            put(c, 0xFB, 1);
            crc = crc_byte(crc, 0xFB);
            for (int i = 0; i < 512; ++i) {
                const std::uint8_t b = pattern(t, s, i);
                put(c, b, 1);
                crc = crc_byte(crc, b);
            }
            put(c, static_cast<std::uint8_t>(crc >> 8), 1);
            put(c, static_cast<std::uint8_t>(crc), 1);
            put(c, 0x4E, 24);
        }
        while (pos_ < kTrackCells) put(c, 0x4E, 1);
    }

private:
    void put(std::vector<std::uint8_t>& c, std::uint8_t b, int n)
    {
        for (int k = 0; k < n && pos_ < kTrackCells; ++k)
            for (int i = 7; i >= 0; --i) {
                const int bit = (b >> i) & 1;
                c[static_cast<std::size_t>(pos_++)] = static_cast<std::uint8_t>(!prev_ && !bit);
                c[static_cast<std::size_t>(pos_++)] = static_cast<std::uint8_t>(bit);
                prev_ = bit;
            }
    }
    void sync(std::vector<std::uint8_t>& c)
    {
        for (int i = 15; i >= 0; --i) c[static_cast<std::size_t>(pos_++)] = (0x4489 >> i) & 1;
        prev_ = 1;
    }

    std::vector<std::vector<std::uint8_t>> cells_;
    int pos_ = 0;
    int prev_ = 0;
};

class Bench {
public:
    Bench()
    {
        V& x = m();
        x.p3 = 1; x.p2 = 1; x.p4 = 1;           // -CS -WE -RE
        x.p22 = 1; x.p23 = 1; x.p37 = 0;        // -TEST HLT -DDEN (MFM)
        x.p32 = 1; x.p36 = 1; x.p33_in = 1;     // READY -WPRT -WF
        x.p19 = 0;                              // -MR
        run(100);
        x.p19 = 1;
    }

    V& m() { return *m_; }
    Disk disk;
    int head = 5;                               // the head's track

    void run(std::uint64_t ticks)
    {
        for (std::uint64_t i = 0; i < ticks; ++i) tick();
    }
    void run_us(std::uint64_t us) { run(us * kTicksPerUs); }

    // runs until INTRQ (or a budget in us), serving DRQ with `serve`
    template <class Serve>
    bool until_intrq(std::uint64_t budget_us, Serve serve)
    {
        for (std::uint64_t i = 0; i < budget_us * kTicksPerUs; ++i) {
            tick();
            if (m().p38) serve();
            if (m().p39) return true;
        }
        return false;
    }
    bool until_intrq(std::uint64_t budget_us)
    {
        return until_intrq(budget_us, [] {});
    }

    void write(int a, std::uint8_t v)
    {
        V& x = m();
        x.p5 = a & 1; x.p6 = (a >> 1) & 1;
        CData* d[8] = {&x.p7_in, &x.p8_in, &x.p9_in, &x.p10_in, &x.p11_in, &x.p12_in, &x.p13_in, &x.p14_in};
        for (int i = 0; i < 8; ++i) *d[i] = (v >> i) & 1u;
        x.p3 = 0; x.p2 = 0;
        run(3);
        x.p2 = 1;
        run(1);
        x.p3 = 1;
        run(1);
    }

    std::uint8_t read(int a)
    {
        V& x = m();
        x.p5 = a & 1; x.p6 = (a >> 1) & 1;
        x.p3 = 0; x.p4 = 0;
        run(2);
        const CData* d[8] = {&x.p7, &x.p8, &x.p9, &x.p10, &x.p11, &x.p12, &x.p13, &x.p14};
        std::uint8_t v = 0;
        for (int i = 0; i < 8; ++i) v = static_cast<std::uint8_t>(v | (*d[i] & 1u) << i);
        x.p4 = 1;
        run(1);
        x.p3 = 1;
        run(1);
        return v;
    }

    std::uint64_t now_us() const { return t_ / kTicksPerUs; }

private:
    void tick()
    {
        V& x = m();
        const std::uint64_t us = t_ / kTicksPerUs;
        x.p24 = (t_ % kTicksPerUs) < kTicksPerUs / 2;                   // CLK 1 MHz
        const std::uint64_t cell = (us / 2) % kTrackCells;
        const int phase = static_cast<int>(t_ % (2 * kTicksPerUs));     // within the 2 us window
        x.p26 = (us / 2) & 1;                                           // RCLK: a window a half
        const bool one = disk.track(head)[cell] != 0;
        x.p27 = !(one && phase >= 6 && phase < 10);                     // the pulse mid-window
        x.p35 = !(us % 200000 < 20);                                    // -IP: 20 us a turn
        x.p34 = head != 0;                                              // -TR00
        if (x.p15 && !step_was_) {
            head += x.p16 ? 1 : -1;
            if (head < 0) head = 0;
            if (head > 79) head = 79;
        }
        step_was_ = x.p15;
        if (x.p30) {                                                    // WG: record the cells
            if (cell != wcell_) {
                disk.track(head)[wcell_] = wpulse_;
                wcell_ = cell;
                wpulse_ = 0;
            }
            if (x.p31) wpulse_ = 1;
        } else
            wcell_ = cell;
        x.clk = 0;
        x.eval();
        x.clk = 1;
        x.eval();
        ++t_;
    }

    std::unique_ptr<V> m_ = std::make_unique<V>();
    std::uint64_t t_ = 0;
    std::uint64_t wcell_ = 0;
    std::uint8_t wpulse_ = 0;
    bool step_was_ = false;
};

// the data bytes of one Read Sector or Read Address
std::vector<std::uint8_t> read_command(Bench& b, std::uint8_t cmd, std::uint64_t budget_us)
{
    std::vector<std::uint8_t> out;
    b.write(0, cmd);
    REQUIRE(b.until_intrq(budget_us, [&] { out.push_back(b.read(3)); }));
    return out;
}

}  // namespace

TEST_CASE("КР1818ВГ93: master reset runs Restore to track 0")
{
    Bench b;
    REQUIRE(b.until_intrq(400000));
    CHECK(b.head == 0);
    CHECK(b.read(1) == 0);                      // TR
    CHECK(b.read(2) == 1);                      // SR after MR
    CHECK((b.read(0) & 0x05) == 0x04);          // TRACK 0, not BUSY
    CHECK(!b.m().p39);                          // the status read cleared INTRQ
}

TEST_CASE("КР1818ВГ93: Seek with verify; Step In, Step Out")
{
    Bench b;
    REQUIRE(b.until_intrq(400000));
    b.write(3, 10);                             // DR: the track to seek
    const std::uint64_t t0 = b.now_us();
    b.write(0, 0x1C);                           // Seek, h, V, 6 ms
    REQUIRE(b.until_intrq(600000));
    CHECK(b.head == 10);
    CHECK(b.read(1) == 10);
    CHECK((b.read(0) & 0x18) == 0);             // no seek or CRC error
    CHECK(b.now_us() - t0 > 10 * 6000);         // 10 steps of 6 ms, settling, an ID
    b.write(0, 0x50);                           // Step In, u
    REQUIRE(b.until_intrq(100000));
    CHECK(b.head == 11);
    CHECK(b.read(1) == 11);
    b.write(0, 0x60);                           // Step Out, no u
    REQUIRE(b.until_intrq(100000));
    CHECK(b.head == 10);
    CHECK(b.read(1) == 11);
}

TEST_CASE("КР1818ВГ93: Read Sector gives the sector's 512 bytes; Read Address its ID")
{
    Bench b;
    REQUIRE(b.until_intrq(400000));
    b.write(3, 3);
    b.write(0, 0x10);                           // Seek track 3, no verify
    REQUIRE(b.until_intrq(400000));
    b.read(0);
    b.write(2, 7);                              // SR = 7
    const std::vector<std::uint8_t> data = read_command(b, 0x80, 1200000);
    REQUIRE(data.size() == 512);
    for (int i = 0; i < 512; ++i)
        CHECK(data[static_cast<std::size_t>(i)] == pattern(3, 7, i));
    CHECK((b.read(0) & 0x1F) == 0);             // no RNF, CRC or lost data
    const std::vector<std::uint8_t> id = read_command(b, 0xC0, 1200000);
    REQUIRE(id.size() == 6);
    CHECK(id[0] == 3);
    CHECK(id[3] == 2);
    CHECK(b.read(2) == 3);                      // the ID's track into SR
}

TEST_CASE("КР1818ВГ93: a sector not on the track is RECORD NOT FOUND")
{
    Bench b;
    REQUIRE(b.until_intrq(400000));
    b.write(2, 11);
    b.write(0, 0x80);
    REQUIRE(b.until_intrq(1500000));
    CHECK((b.read(0) & 0x10) != 0);
}

TEST_CASE("КР1818ВГ93: Write Sector, then the sector reads back")
{
    Bench b;
    REQUIRE(b.until_intrq(400000));
    b.write(2, 5);
    std::size_t n = 0;
    b.write(0, 0xA0);                           // Write Sector
    REQUIRE(b.until_intrq(1200000, [&] { b.write(3, static_cast<std::uint8_t>(n++ * 5 + 1)); }));
    CHECK(n == 512);
    CHECK((b.read(0) & 0x5C) == 0);
    const std::vector<std::uint8_t> data = read_command(b, 0x80, 1200000);
    REQUIRE(data.size() == 512);
    for (std::size_t i = 0; i < 512; ++i)
        CHECK(data[i] == static_cast<std::uint8_t>(i * 5 + 1));
    CHECK((b.read(0) & 0x1C) == 0);
    b.write(2, 6);                              // the next sector is untouched
    const std::vector<std::uint8_t> next = read_command(b, 0x80, 1200000);
    REQUIRE(!next.empty());
    CHECK(next[0] == pattern(0, 6, 0));
}

TEST_CASE("КР1818ВГ93: Force Interrupt ends a command")
{
    Bench b;
    REQUIRE(b.until_intrq(400000));
    b.read(0);
    b.write(3, 40);
    b.write(0, 0x13);                           // a long seek, 30 ms steps
    b.run_us(50000);
    CHECK((b.read(0) & 0x01) != 0);             // BUSY
    b.write(0, 0xD8);                           // Force Interrupt, at once
    b.run_us(10);
    CHECK((b.read(0) & 0x01) == 0);
    CHECK(b.m().p39);
}
