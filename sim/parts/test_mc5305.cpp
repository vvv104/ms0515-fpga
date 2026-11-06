// mc5305: the floppy drive as logic, against the bench's drive (sim/board/
// drive.h) tick by tick: the index, track 0, ready and the flux pulses on
// the same lines (the flux a master tick later, as mc5305.sv puts it out),
// from an image served through MiSTer's block interface.
// After the head moves, or the side changes, the logic loads the track and
// works its way to the head's place in the turn; the pulses are compared
// again once it has (kSettle, inside the 15 ms a drive's head takes to
// settle, which the controller waits).
#include <doctest/doctest.h>

#include "Vmc5305.h"
#include "../board/block_device.h"
#include "../board/drive.h"
#include "../board/logic_drive.h"

#include <algorithm>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace {

constexpr std::uint64_t kUs = 120;
constexpr std::uint64_t kSettle = 3000 * kUs;

// an image whose every byte tells where it is
std::vector<std::uint8_t> image(int sides)
{
    std::vector<std::uint8_t> im(static_cast<std::size_t>(sides) * drive::kTracks * drive::kSectors *
                                 drive::kSectorBytes);
    for (std::size_t i = 0; i < im.size(); ++i)
        im[i] = static_cast<std::uint8_t>(i * 7 + i / 512 * 13 + i / 5120);
    return im;
}

class Bench {
public:
    Bench(std::vector<std::uint8_t> im, int layout, bool protect = false)
        : device_(im), ref_(std::move(im), layout_of(layout), protect)
    {
        m_.layout = static_cast<CData>(layout);
        m_.img_size = device_.image().size();
        m_.img_readonly = protect ? 1 : 0;
        m_.select0 = 1;
        m_.motor = 1;
        m_.direction = 1;
        m_.step = 1;
        m_.write_data = 1;
        m_.write_gate = 1;
        m_.side = 1;
        m_.sd_ack = 0;
        m_.sd_buff_wr = 0;
        m_.img_mounted = 1;
        tick();
        m_.img_mounted = 0;
    }

    // ticks with the lines as set; the outputs compared while `compare`
    void run(std::uint64_t ticks, bool compare)
    {
        for (std::uint64_t i = 0; i < ticks; ++i) {
            tick();
            if (!m_.index || !m_.ready || !m_.data || !m_.protect)
                ++pulled;
            if (device_.image().empty())                        // no disk: the bench's drive has none
                continue;
            const drive::Outputs o = ref_.tick(t_, lines_);
            const bool data_was = ref_data_;            // the flux a tick later (mc5305.sv)
            ref_data_ = o.data;
            if (!compare)
                continue;
            ++compared;
            const bool same = o.index == (m_.index != 0) && o.track0 == (m_.track0 != 0) &&
                              o.ready == (m_.ready != 0) && o.protect == (m_.protect != 0) &&
                              data_was == (m_.data != 0);
            if (!same) {
                if (differ == 0) first_differ_at = t_;
                last_differ_at = t_;
            }
            if (!same && ++differ <= 5)
                MESSAGE("tick " << t_ << ": ref index " << o.index << " track0 " << o.track0 << " ready " << o.ready
                                << " data " << o.data << "; logic " << int(m_.index) << " " << int(m_.track0) << " "
                                << int(m_.ready) << " " << int(m_.data));
        }
    }

    void select(bool on) { lines_.select0 = !on; m_.select0 = lines_.select0; }
    void motor(bool on) { lines_.motor = !on; m_.motor = lines_.motor; }
    void side(int s) { lines_.side = s == 0; m_.side = lines_.side; }
    // one step pulse (ШАГ L low 4 us), inward or out
    void step(bool in)
    {
        lines_.direction = !in;
        m_.direction = lines_.direction;
        run(10 * kUs, false);
        lines_.step = false;
        m_.step = 0;
        run(4 * kUs, false);
        lines_.step = true;
        m_.step = 1;
        run(3000 * kUs, false);
    }
    int head() const { return ref_.head(); }

    // the controller writing from tick `start` on: РЗП on, a ДЗП fall for
    // each cell that turns, its clock a little off the drive's (up to 20
    // ticks early or late), РЗП off a cell after the last
    void write(std::uint64_t start, const std::vector<std::uint8_t>& cells, bool compare = false)
    {
        run(start - t_, false);
        set_gate(false);
        const std::uint64_t from = t_;
        for (std::size_t i = 0; i < cells.size(); ++i) {
            const std::uint64_t at = from + i * drive::Mc5305::kCellTicks + 20 + (i * 37) % 41 - 20;
            run(at - t_, compare);
            if (cells[i]) {
                set_data(false);
                run(8, compare);
                set_data(true);
            }
        }
        run(drive::Mc5305::kCellTicks, compare);
        set_gate(true);
        write_end = t_;
    }
    // the tick of the next index, and of a byte of the turn after it
    std::uint64_t next_index() const { return ref_.turn_start(t_) + drive::Mc5305::kTurnTicks; }
    const std::vector<std::uint8_t>& image() const { return device_.image(); }
    int loads() const { return device_.reads; }
    int writes() const { return device_.writes; }
    const std::vector<std::uint8_t>& ref_image() const { return ref_.image(); }

    std::uint64_t compared = 0, differ = 0;
    std::uint64_t first_differ_at = 0, last_differ_at = 0;      // the ticks of the first and the last difference
    std::uint64_t write_end = 0;        // the tick the last write's gate went off
    std::uint64_t pulled = 0;           // ticks with the index, ready, data or protect line pulled low

private:
    // a master tick, the block device answering as MiSTer's does
    void tick()
    {
        device_.serve(m_);
        m_.clk = 0;
        m_.eval();
        m_.clk = 1;
        m_.eval();
        ++t_;
    }

    void set_gate(bool high) { lines_.write_gate = high; m_.write_gate = high; }
    void set_data(bool high) { lines_.write_data = high; m_.write_data = high; }
    static drive::Layout layout_of(int layout)
    {
        return layout == 0 ? drive::omega() : layout == 1 ? drive::vvv() : drive::fdz();
    }


    BlockDevice<Vmc5305> device_;
    drive::Mc5305 ref_;
    drive::Lines lines_;
    std::unique_ptr<Vmc5305> model_ = std::make_unique<Vmc5305>();
    Vmc5305& m_ = *model_;
    std::uint64_t t_ = 0;
    bool ref_data_ = true;
};

constexpr std::uint64_t kTurn = drive::Mc5305::kTurnTicks;

}  // namespace

TEST_CASE("mc5305: unselected, every line let go")
{
    Bench b(image(1), 1);
    b.motor(true);
    b.run(kTurn / 4, true);
    CHECK(b.differ == 0);
}

TEST_CASE("mc5305: no disk in - nothing asked of the block device, no index, no ready, no flux")
{
    Bench b({}, 1);
    b.select(true);
    b.motor(true);
    b.run(3 * kTurn, false);
    CHECK(b.loads() == 0);
    CHECK(b.pulled == 0);
}

TEST_CASE("mc5305: the turns of track 0 from the motor's start, as the bench's drive")
{
    for (const int layout : {0, 1, 2}) {
        INFO("layout " << layout);
        Bench b(image(2), layout);
        b.select(true);
        b.run(kSettle, false);              // the track loaded while the motor is still
        b.motor(true);
        b.run(3 * kTurn + kTurn / 2, true); // index, ready from the third, the flux
        CHECK(b.compared > 0);
        CHECK(b.differ == 0);
    }
}

TEST_CASE("mc5305: the head stepped in and out, the side changed, a single-sided image")
{
    Bench b(image(1), 1);
    b.select(true);
    b.motor(true);
    b.run(kSettle, false);
    b.step(true);
    b.step(true);
    REQUIRE(b.head() == 2);
    b.run(kSettle, false);
    b.run(kTurn / 3, true);
    b.side(1);                              // no second side: side 0 again
    b.run(kSettle, false);
    b.run(kTurn / 5, true);
    b.step(false);
    REQUIRE(b.head() == 1);
    b.run(kSettle, false);
    b.run(kTurn / 3, true);
    CHECK(b.differ == 0);
    // tracks 0, 1, 2 and 1 again, ten blocks each; the side alone loads nothing here
    CHECK(b.loads() == 4 * drive::kSectors);
}

namespace {

// the drive selected, turning and ready
void spin_up(Bench& b)
{
    b.select(true);
    b.motor(true);
    b.run(3 * kTurn, false);
}

std::vector<std::uint8_t> sector_of(std::uint8_t seed)
{
    std::vector<std::uint8_t> s(drive::kSectorBytes);
    for (std::size_t i = 0; i < s.size(); ++i)
        s[i] = static_cast<std::uint8_t>(i * 3 + seed);
    return s;
}

constexpr std::uint64_t kByte = 16 * drive::Mc5305::kCellTicks;

}  // namespace

TEST_CASE("mc5305: a sector written where its data field is - into the image, read back")
{
    const drive::Layout layout = drive::vvv();
    Bench b(image(2), 1);
    spin_up(b);
    const std::vector<std::uint8_t> data = sector_of(0x55);
    drive::Encoder e;                                   // as the FD1793 writes a sector
    drive::field(e, layout.data_sync, 0xFB, data.data(), data.size());
    e.byte(0xFF);
    b.write(b.next_index() + static_cast<std::uint64_t>(drive::data_field_at(layout, 3)) * kByte, e.cells);
    b.run(kSettle, false);
    CHECK(b.writes() == 1);
    CHECK(b.image() == b.ref_image());
    CHECK(std::equal(data.begin(), data.end(), b.image().begin() + 3 * drive::kSectorBytes));
    b.run(kTurn, true);                                 // the new sector under the head
    CHECK(b.differ == 0);
}

TEST_CASE("mc5305: the lines while a sector is written and after it, as the bench's drive")
{
    const drive::Layout layout = drive::vvv();
    Bench b(image(2), 1);
    spin_up(b);
    const std::vector<std::uint8_t> data = sector_of(0x55);
    drive::Encoder e;
    drive::field(e, layout.data_sync, 0xFB, data.data(), data.size());
    e.byte(0xFF);
    b.write(b.next_index() + static_cast<std::uint64_t>(drive::data_field_at(layout, 3)) * kByte, e.cells, true);
    b.run(2 * kTurn, true);
    CHECK(b.differ == 0);
}

TEST_CASE("mc5305: each sector of a track written by its place - a single-sided image, each layout")
{
    for (const int code : {0, 1, 2}) {
        INFO("layout " << code);
        const drive::Layout layout = code == 0 ? drive::omega() : code == 1 ? drive::vvv() : drive::fdz();
        Bench b(image(1), code);
        spin_up(b);
        b.step(true);
        b.step(true);
        b.step(true);
        REQUIRE(b.head() == 3);
        b.run(kSettle, false);
        for (int k = 0; k < drive::kSectors; ++k) {
            INFO("sector's place " << k);
            const std::vector<std::uint8_t> data = sector_of(static_cast<std::uint8_t>(0x21 + k * 29));
            drive::Encoder e;
            drive::field(e, layout.data_sync, 0xFB, data.data(), data.size());
            e.byte(0xFF);
            // the controller starts a byte and a part of one after the field's place
            const std::uint64_t at = static_cast<std::uint64_t>(drive::data_field_at(layout, k)) * kByte + kByte + 700;
            b.write(b.next_index() + at, e.cells, true);
            b.run(kSettle, true);
            CHECK(b.writes() == k + 1);
            REQUIRE(b.image() == b.ref_image());
            const std::size_t sector = static_cast<std::size_t>(layout.order[static_cast<std::size_t>(k)]) - 1;
            CHECK(std::equal(data.begin(), data.end(),
                             b.image().begin() + (3 * drive::kSectors + sector) * drive::kSectorBytes));
        }
        b.run(kTurn, true);
        CHECK(b.differ == 0);
    }
}

TEST_CASE("mc5305: four sectors written one after another in a turn, each by its place")
{
    for (const int code : {0, 1}) {
        INFO("layout " << code);
        const drive::Layout layout = code == 0 ? drive::omega() : drive::vvv();
        Bench b(image(1), code);
        spin_up(b);
        const std::uint64_t index = b.next_index();
        for (int k = 4; k < 8; ++k) {
            const std::vector<std::uint8_t> data = sector_of(static_cast<std::uint8_t>(0x37 + k * 31));
            drive::Encoder e;
            drive::field(e, layout.data_sync, 0xFB, data.data(), data.size());
            e.byte(0xFF);
            b.write(index + static_cast<std::uint64_t>(drive::data_field_at(layout, k)) * kByte + kByte + 700, e.cells,
                    true);
        }
        b.run(kSettle, true);
        CHECK(b.writes() == 4);
        CHECK(b.image() == b.ref_image());
        CHECK(b.image() != image(1));
        b.run(kTurn, true);
        CHECK(b.differ == 0);
    }
}

TEST_CASE("mc5305: a sector written before its place - the flux apart only up to the field's end")
{
    // The bench's drive shows the new sector as the gate goes off, the
    // logic from its next field: what is left of the layout's field under
    // the head differs, and nothing after it.
    const drive::Layout layout = drive::omega();
    std::uint64_t apart = 0;
    for (const std::uint64_t early : {4400u, 4800u, 7000u, 3 * 3840u}) {
        INFO("early by " << early << " ticks");
        Bench b(image(1), 0);
        spin_up(b);
        const std::vector<std::uint8_t> data = sector_of(0x55);
        drive::Encoder e;
        drive::field(e, layout.data_sync, 0xFB, data.data(), data.size());
        e.byte(0xFF);
        b.write(b.next_index() + static_cast<std::uint64_t>(drive::data_field_at(layout, 3)) * kByte - early, e.cells,
                true);
        b.run(kTurn, true);
        CHECK(b.writes() == 1);
        CHECK(b.image() == b.ref_image());
        if (b.differ != 0) {                            // by the sector's bytes: the two CRCs' cells
            CHECK(b.first_differ_at >= b.write_end);
            CHECK(b.last_differ_at < b.write_end + kAfterWrite);
        }
        apart += b.differ;
    }
    CHECK(apart > 0);
}

TEST_CASE("mc5305: a sector written with nothing after its CRC, the gate off a cell later")
{
    // a field is whole with its CRC's last cell, be it 0 or 1 and whatever comes after
    const drive::Layout layout = drive::omega();
    int ending_0 = 0, ending_1 = 0;
    for (int seed = 1; seed < 40 && (ending_0 < 2 || ending_1 < 2); ++seed) {
        const std::vector<std::uint8_t> data = sector_of(static_cast<std::uint8_t>(seed * 7));
        std::vector<std::uint8_t> f = {0xA1, 0xA1, 0xA1, 0xFB};
        f.insert(f.end(), data.begin(), data.end());
        const bool last = (drive::crc(f.data(), f.size()) & 1) != 0;
        if ((last ? ending_1 : ending_0) >= 2)
            continue;
        ++(last ? ending_1 : ending_0);
        INFO("seed " << seed << ", the CRC's last bit " << last);
        Bench b(image(1), 0);
        spin_up(b);
        drive::Encoder e;
        drive::field(e, layout.data_sync, 0xFB, data.data(), data.size());
        b.write(b.next_index() + static_cast<std::uint64_t>(drive::data_field_at(layout, 3)) * kByte, e.cells, true);
        b.run(kSettle, false);
        CHECK(b.writes() == 1);
        CHECK(b.image() == b.ref_image());
        CHECK(std::equal(data.begin(), data.end(), b.ref_image().begin() + 3 * drive::kSectorBytes));
        b.run(kTurn, true);
        CHECK(b.last_differ_at < b.write_end + kAfterWrite);
    }
    CHECK(ending_0 == 2);
    CHECK(ending_1 == 2);
}

TEST_CASE("mc5305: a track written from the index (Write Track) - every sector into the image")
{
    const drive::Layout layout = drive::omega();
    Bench b(image(2), 0);
    b.side(1);
    spin_up(b);
    std::vector<std::uint8_t> sectors;
    for (int s = 0; s < drive::kSectors; ++s) {
        const std::vector<std::uint8_t> one = sector_of(static_cast<std::uint8_t>(s * 17));
        sectors.insert(sectors.end(), one.begin(), one.end());
    }
    std::vector<std::uint8_t> cells = drive::track(layout, 0, 1, sectors.data());
    cells.resize(cells.size() - 64);                    // РЗП off before the index
    b.write(b.next_index(), cells);
    b.run(kSettle, false);
    CHECK(b.writes() == drive::kSectors);
    CHECK(b.image() == b.ref_image());
    CHECK(std::equal(sectors.begin(), sectors.end(), b.image().begin() + 5120));
    b.run(kTurn, true);
    CHECK(b.differ == 0);
}

TEST_CASE("mc5305: a protected disk takes nothing")
{
    Bench b(image(2), 1, true);
    spin_up(b);
    std::vector<std::uint8_t> sectors(drive::kSectors * drive::kSectorBytes, 0xE5);
    std::vector<std::uint8_t> cells = drive::track(drive::vvv(), 0, 0, sectors.data());
    cells.resize(cells.size() - 64);
    b.write(b.next_index(), cells);
    b.run(kSettle, false);
    CHECK(b.writes() == 0);
    CHECK(b.image() == image(2));
}

TEST_CASE("logic_drive: the blocks where two images differ, for the run's report")
{
    std::vector<std::uint8_t> a = image(1), b = a;
    CHECK(differing_blocks(a, b).empty());
    b[3 * 512 + 7] ^= 0x10;                             // block 3: one byte
    b[3 * 512 + 500] ^= 0x01;                           // ... and another
    b[799 * 512] ^= 0x80;                               // the last block's first
    const std::vector<BlockDifference> d = differing_blocks(a, b);
    REQUIRE(d.size() == 2);
    CHECK(d[0].block == 3);
    CHECK(d[0].bytes == 2);
    CHECK(d[0].first == 7);
    CHECK(d[1].block == 799);
    CHECK(d[1].bytes == 1);
    CHECK(d[1].first == 0);
}
