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

// The interface's lines at the drive's connector, each true high: the
// board's lines active low (Shugart), the drive's open collector, high
// when let go.
struct Lines {
    bool select0 = true, select1 = true, select2 = true, select3 = true;    // ВН0..ВН3 L
    bool motor = true;          // МВК L
    bool direction = true;      // НПШ: high out, toward track 0
    bool step = true;           // ШАГ L
    bool write_data = true;     // ДЗП L: each fall turns the flux
    bool write_gate = true;     // РЗП L
    bool side = true;           // ВПВ: high the lower head, side 0
};

struct Outputs {
    bool index = true;          // ИНД L
    bool track0 = true;         // Д00 L
    bool protect = true;        // ЗЗП L
    bool data = true;           // ДВС L: a pulse a flux transition
    bool ready = true;          // НГТ L
};

// «Электроника МС 5305» (ТО 2.791.064 Табл. 9), drive 0 of the board's
// four selects.  Sources: «Ремонт дисководов» (Солон 1995), ch. 1, on the
// МС 5305 [BOOK]; the TEAC FD-55 specification, its 80-track twin, where
// the МС 5305's own papers give no figure [TEAC].
// - The spindle turns at 300 rpm from МВК, select or not [BOOK p.3, the
//   schematic]; spin-up is not modelled: it turns at speed at once.
// - ИНД: 4 ms low a turn [TEAC 2..5.5 ms]; the first as the motor starts.
// - НГТ: on at the third index after the start, off when no index has come
//   for 450 ms [BOOK pp.7-9, the one-shot DD7.2].
// - ШАГ: the head moves on its trailing edge, when selected and not
//   writing; НПШ high is out [BOOK pp.3, 6, 9]; tracks 0..kTracks-1.
// - ДВС: 1 us low a flux transition, from the cell's start [TEAC 1 ±0.5 us].
// - ДЗП: a fall turns the flux; recorded a cell at a time while РЗП is on.
// - The outputs answer only when selected [BOOK p.3].
class Mc5305 {
public:
    static constexpr std::uint64_t kTicksPerUs = 120;
    static constexpr std::uint64_t kCellTicks = 2 * kTicksPerUs;
    static constexpr std::uint64_t kTurnTicks = kCellsPerTurn * kCellTicks;
    static constexpr std::uint64_t kIndexTicks = 4000 * kTicksPerUs;
    static constexpr std::uint64_t kReadyLossTicks = 450'000 * kTicksPerUs;
    static constexpr std::uint64_t kPulseTicks = kTicksPerUs;

    // image: 80 tracks of one side (409 600 bytes), or of two with the
    // sides of a track one after the other (819 200)
    Mc5305(std::vector<std::uint8_t> image, Layout layout, bool protect = false)
        : image_(std::move(image)), layout_(layout), protect_(protect),
          sides_(image_.size() >= 2u * kTracks * kSectors * kSectorBytes ? 2 : 1)
    {
    }

    Outputs tick(std::uint64_t t, const Lines& in)
    {
        spindle(t, in);
        const bool selected = !in.select0;
        if (selected) head_moves(in);
        step_was_ = in.step;
        Outputs o;
        if (!selected) return o;
        o.index = !(turning_ && phase(t) < kIndexTicks);
        o.track0 = head_ != 0;
        o.protect = !protect_;
        o.ready = !ready_;
        if (turning_) o.data = !pulse(t, in.side ? 0 : 1);
        return o;
    }

    int head() const { return head_; }

    // everything that changes as it runs, for a snapshot (sim/board/snapshot.h)
    template <class Archive>
    void state(Archive& a)
    {
        a(head_);
        a(turning_);
        a(ready_);
        a(step_was_);
        a(started_);
        a(last_index_);
        a(indexes_);
        a.bytes(image_);
        cached_ = -1;                                   // the track is made again from the image
    }
    // the tick the turn in progress at t began, at its index
    std::uint64_t turn_start(std::uint64_t t) const { return t - phase(t); }

private:
    std::uint64_t phase(std::uint64_t t) const { return (t - started_) % kTurnTicks; }

    void spindle(std::uint64_t t, const Lines& in)
    {
        const bool on = !in.motor;
        if (on && !turning_)
            started_ = t;                               // the first index as it starts
        turning_ = on;
        if (turning_) {                                 // an index a turn, at its start
            const std::uint64_t turns = (t - started_) / kTurnTicks;
            indexes_ = turns + 1;
            last_index_ = started_ + turns * kTurnTicks;
        }
        ready_ = indexes_ >= 3 && t - last_index_ < kReadyLossTicks;
    }

    void head_moves(const Lines& in)
    {
        if (in.step && !step_was_ && in.write_gate) {   // the trailing edge, not writing
            if (in.direction) head_ = head_ > 0 ? head_ - 1 : 0;
            else head_ = head_ < kTracks - 1 ? head_ + 1 : kTracks - 1;
        }
    }

    bool pulse(std::uint64_t t, int side)
    {
        const std::uint64_t p = phase(t);
        return (p % kCellTicks) < kPulseTicks && cells(side)[static_cast<std::size_t>(p / kCellTicks)] != 0;
    }

    const std::vector<std::uint8_t>& cells(int side)
    {
        if (side >= sides_) side = 0;
        const int key = head_ * 2 + side;
        if (key != cached_) {
            const std::size_t at = (static_cast<std::size_t>(head_) * sides_ + side) * kSectors * kSectorBytes;
            track_ = track(layout_, head_, side, image_.data() + at);
            cached_ = key;
        }
        return track_;
    }

    std::vector<std::uint8_t> image_;
    Layout layout_;
    bool protect_;
    int sides_;
    int head_ = 0;
    bool turning_ = false, ready_ = false, step_was_ = true;
    std::uint64_t started_ = 0, last_index_ = 0, indexes_ = 0;
    int cached_ = -1;
    std::vector<std::uint8_t> track_;
};

}  // namespace drive
