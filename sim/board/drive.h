// drive: the MS 0515's floppy drive as the bench's, outside the board like
// the supply.  First its tracks: a side's bare sectors (the images hold
// nothing else) laid out as a formatter writes them with the FD1793's
// Write Track, and MFM-encoded into the flux cells of one revolution - a
// cell 2 us at 250 kbit/s, 100 000 cells in the 200 ms of a turn at
// 300 rpm.  A cell is 1 where the flux changes.
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <vector>

namespace drive {

constexpr int kTracks = 80;
constexpr int kSectors = 10;
constexpr int kSectorBytes = 512;
constexpr std::size_t kCellsPerTurn = 100'000;      // 200 ms / 2 us

// What a formatter puts on a track with the FD1793's Write Track: the
// counts of the fields of the System 34 layout and the sectors' order.
// The defaults are the data sheet's minimums; omega(), vvv() and fdz()
// below are what the MS 0515's own formatters write.
struct Layout {
    enum class Mark { none, c2_c2_c2_fc, c2_c2_c2_c2 };

    int gap1 = 32;              // 4E after the index
    Mark index_mark = Mark::none;
    int index_sync = 12;        // 00 before the index mark
    int gap_after_mark = 0;     // 4E after it
    int id_sync = 12;           // 00 before an ID's A1 A1 A1
    int data_sync = 12;         // 00 before a data field's
    int gap2 = 22;              // 4E between an ID and its data
    int gap3 = 24;              // 4E after a data field
    bool h_is_side = true;      // H in the IDs: the side, or 0 on both sides
    std::array<int, kSectors> order = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
};

// ОМЕГА's FORML/FORMH (the collection's software/format/omega, from disks
// 059, 062, 063, 172): the buffer built from 001146 on.  Four C2s and no
// FC; H 0 on the upper side too (FORMH 001266 CLR); 6245 bytes before
// Gap IV, of the 6250 a turn holds.
inline Layout omega()
{
    Layout l;
    l.gap1 = 54;
    l.index_mark = Layout::Mark::c2_c2_c2_c2;
    l.gap_after_mark = 35;
    l.id_sync = 8;
    l.gap3 = 44;
    l.h_is_side = false;
    return l;
}

// The same program reassembled (software/format/vvv; disk4's FORML.EXE and
// FORMH.EXE): the standard index mark, H the side.
inline Layout vvv()
{
    Layout l;
    l.gap1 = 80;
    l.index_mark = Layout::Mark::c2_c2_c2_fc;
    l.gap_after_mark = 50;
    l.gap3 = 30;
    return l;
}

// FDZ.SAV, Mihin-soft & SPF Sensor 1990 (disk1.dsk, disk2.dsk): no index
// mark, the fields streamed from its inline list at 001702 on.
inline Layout fdz()
{
    Layout l;
    l.gap1 = 34;
    l.id_sync = 8;
    l.gap3 = 45;
    return l;
}

// The bytes a layout puts before Gap IV, which fills the turn to the index.
inline int bytes_before_gap4(const Layout& l)
{
    const int mark = l.index_mark == Layout::Mark::none ? 0 : l.index_sync + 4 + l.gap_after_mark;
    const int id = l.id_sync + 3 + 1 + 4 + 2;
    const int data = l.data_sync + 3 + 1 + kSectorBytes + 2;
    return l.gap1 + mark + kSectors * (id + l.gap2 + data + l.gap3);
}

// CRC-CCITT (x^16 + x^12 + x^5 + 1) from FFFF, as the FD1793 computes it
// over a field from its first A1.
inline std::uint16_t crc(const std::uint8_t* p, std::size_t n)
{
    std::uint16_t c = 0xFFFF;
    for (std::size_t i = 0; i < n; ++i) {
        c = static_cast<std::uint16_t>(c ^ (p[i] << 8));
        for (int b = 0; b < 8; ++b)
            c = static_cast<std::uint16_t>((c & 0x8000) ? (c << 1) ^ 0x1021 : c << 1);
    }
    return c;
}

// MFM cells, a byte at a time: a data bit's cell is the bit, the clock cell
// before it is 1 between two zeros.  A mark byte (A1, C2) leaves out one
// clock, which is how the controller finds it: A1 is 4489, C2 is 5224.
class Encoder {
public:
    std::vector<std::uint8_t> cells;

    void byte(std::uint8_t b) { put(b, -1); }
    void bytes(std::uint8_t b, int n)
    {
        for (int i = 0; i < n; ++i) byte(b);
    }
    void a1() { put(0xA1, 2); }     // the clock before data bit 2 left out: 4489
    void c2() { put(0xC2, 3); }     // the clock before data bit 3: 5224

private:
    void put(std::uint8_t b, int no_clock_before)
    {
        for (int i = 7; i >= 0; --i) {
            const int bit = (b >> i) & 1;
            const int clock = (!last_ && !bit && i != no_clock_before) ? 1 : 0;
            cells.push_back(static_cast<std::uint8_t>(clock));
            cells.push_back(static_cast<std::uint8_t>(bit));
            last_ = bit;
        }
    }
    int last_ = 0;
};

// One field from its A1s: the bytes after them go through the CRC with them.
inline void field(Encoder& e, int sync, std::uint8_t mark, const std::uint8_t* p, std::size_t n)
{
    e.bytes(0x00, sync);
    std::vector<std::uint8_t> f = {0xA1, 0xA1, 0xA1, mark};
    f.insert(f.end(), p, p + n);
    const std::uint16_t c = crc(f.data(), f.size());
    e.a1();
    e.a1();
    e.a1();
    for (std::size_t i = 3; i < f.size(); ++i) e.byte(f[i]);
    e.byte(static_cast<std::uint8_t>(c >> 8));
    e.byte(static_cast<std::uint8_t>(c));
}

// The cells of track `track`, side `side`, from that side's 10 sectors of
// 512 bytes in sector order (sector 1 first).
inline std::vector<std::uint8_t> track(const Layout& l, int track, int side, const std::uint8_t* sectors)
{
    Encoder e;
    e.bytes(0x4E, l.gap1);
    if (l.index_mark != Layout::Mark::none) {
        e.bytes(0x00, l.index_sync);
        e.c2();
        e.c2();
        e.c2();
        if (l.index_mark == Layout::Mark::c2_c2_c2_fc)
            e.byte(0xFC);
        else
            e.c2();
        e.bytes(0x4E, l.gap_after_mark);
    }
    const auto h = static_cast<std::uint8_t>(l.h_is_side ? side : 0);
    for (const int r : l.order) {
        const std::uint8_t id[] = {static_cast<std::uint8_t>(track), h, static_cast<std::uint8_t>(r), 2};
        field(e, l.id_sync, 0xFE, id, sizeof id);
        e.bytes(0x4E, l.gap2);
        field(e, l.data_sync, 0xFB, sectors + (r - 1) * kSectorBytes, kSectorBytes);
        e.bytes(0x4E, l.gap3);
    }
    if (e.cells.size() > kCellsPerTurn)
        throw std::length_error("drive: the layout does not fit a revolution");
    while (e.cells.size() < kCellsPerTurn)
        e.byte(0x4E);                                  // Gap IV to the index
    e.cells.resize(kCellsPerTurn);
    return e.cells;
}

}  // namespace drive
