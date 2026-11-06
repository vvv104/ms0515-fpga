// The МС 5305 drive's mechanics through its interface: the spindle and the
// index, READY on the third index, the head stepping on the trailing edge
// of ШАГ, track 0, the read pulses of a track, and writing.
#include <doctest/doctest.h>

#include "drive.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace {

using drive::Lines;
using drive::Mc5305;

constexpr std::uint64_t kUs = 120;                      // master ticks a microsecond
constexpr std::uint64_t kMs = 1000 * kUs;

std::vector<std::uint8_t> image_of(int sides)
{
    std::vector<std::uint8_t> img(static_cast<std::size_t>(sides) * drive::kTracks * drive::kSectors *
                                  drive::kSectorBytes);
    for (std::size_t i = 0; i < img.size(); ++i)
        img[i] = static_cast<std::uint8_t>(i * 13 + i / 512);
    return img;
}

// the board's lines idle: everything high (inactive), but this drive selected
Lines selected()
{
    Lines in;
    in.select0 = false;
    return in;
}

}  // namespace

TEST_CASE("МС 5305: with the motor on, an index a turn and READY on the third")
{
    Mc5305 d(image_of(2), drive::omega());
    Lines in = selected();
    in.motor = false;                                   // МВК L on
    std::vector<std::uint64_t> index_falls;
    std::uint64_t ready_at = 0;
    bool index_was = true;
    for (std::uint64_t t = 0; t < 700 * kMs; t += kUs / 4) {
        const drive::Outputs o = d.tick(t, in);
        if (!o.index && index_was) index_falls.push_back(t);
        index_was = o.index;
        if (!o.ready && ready_at == 0) ready_at = t;
    }
    REQUIRE(index_falls.size() >= 3);
    CHECK(index_falls[1] - index_falls[0] == 200 * kMs);
    CHECK(ready_at == index_falls[2]);                  // the third index
}

TEST_CASE("МС 5305: no motor, no index, not ready")
{
    Mc5305 d(image_of(2), drive::omega());
    const Lines in = selected();
    for (std::uint64_t t = 0; t < 500 * kMs; t += kMs) {
        const drive::Outputs o = d.tick(t, in);
        CHECK(o.index);
        CHECK(o.ready);
    }
}

TEST_CASE("МС 5305: the head steps on the trailing edge of ШАГ; track 0 at 0")
{
    Mc5305 d(image_of(2), drive::omega());
    Lines in = selected();
    std::uint64_t t = 0;
    auto step = [&](bool in_toward_centre) {
        in.direction = !in_toward_centre;               // НПШ high: out, toward track 0
        d.tick(t += kUs, in);
        in.step = false;
        const int before = d.head();
        d.tick(t += kUs, in);
        CHECK(d.head() == before);                      // no move on the leading edge
        in.step = true;
        d.tick(t += kUs, in);
        t += 3 * kMs;
    };
    CHECK(!d.tick(t, in).track0);                       // the head starts at 0
    step(true);
    step(true);
    CHECK(d.head() == 2);
    CHECK(d.tick(t, in).track0);                        // not at 0: high
    step(false);
    step(false);
    step(false);                                        // stops at 0
    CHECK(d.head() == 0);
    CHECK(!d.tick(t, in).track0);
}

TEST_CASE("МС 5305: the read pulses are the track's flux cells, 1 us low each")
{
    const std::vector<std::uint8_t> img = image_of(2);
    Mc5305 d(img, drive::omega());
    Lines in = selected();
    in.motor = false;
    const std::vector<std::uint8_t> cells = drive::track(drive::omega(), 0, 0, img.data() + 0);
    // the first 2000 cells after the index, sampled mid-pulse
    std::uint64_t t = 0;
    while (true) {
        const drive::Outputs o = d.tick(t, in);
        if (!o.index) break;
        t += kUs / 2;
    }
    const std::uint64_t turn_start = d.turn_start(t);
    for (std::size_t c = 0; c < 2000; ++c) {
        const std::uint64_t at = turn_start + c * 2 * kUs + kUs / 2;
        CHECK_MESSAGE(d.tick(at, in).data == (cells[c] == 0), "cell ", c);
    }
}

namespace {

// a controller writing: РЗП on, a ДЗП fall for each cell that turns,
// starting at tick `start`, the writer's clock a little off the drive's
// (each fall up to 20 ticks early or late); РЗП off a cell after
std::uint64_t write(Mc5305& d, Lines& in, std::uint64_t start, const std::vector<std::uint8_t>& cells)
{
    constexpr std::uint64_t kCell = Mc5305::kCellTicks;
    in.write_gate = false;
    std::uint64_t t = start;
    for (std::size_t i = 0; i < cells.size(); ++i) {
        const std::uint64_t at = start + i * kCell + static_cast<std::uint64_t>(20 + (i * 37) % 41) - 20;
        for (; t < at; t += 8) d.tick(t, in);
        if (cells[i]) {
            in.write_data = false;
            d.tick(t, in);
            t += 8;
            in.write_data = true;
        }
    }
    for (const std::uint64_t end = t + kCell; t < end; t += 8) d.tick(t, in);
    in.write_gate = true;
    d.tick(t, in);
    return t;
}

// the drive turning, selected and ready; the tick of the next index
std::uint64_t spin_up(Mc5305& d, Lines& in)
{
    in.motor = false;
    std::uint64_t t = 0;
    for (; d.tick(t, in).ready; t += kUs) {}
    return d.turn_start(t) + Mc5305::kTurnTicks;
}

std::vector<std::uint8_t> sector_of(std::uint8_t seed)
{
    std::vector<std::uint8_t> s(drive::kSectorBytes);
    for (std::size_t i = 0; i < s.size(); ++i)
        s[i] = static_cast<std::uint8_t>(i * 3 + seed);
    return s;
}

}  // namespace

TEST_CASE("МС 5305: a sector written where its data field is goes into the image")
{
    const std::vector<std::uint8_t> img = image_of(2);
    const drive::Layout layout = drive::vvv();
    Mc5305 d(img, layout);
    Lines in = selected();
    const std::uint64_t index = spin_up(d, in);
    // as the FD1793 writes a sector: its sync, A1s, FB, the data, the CRC, FF
    const std::vector<std::uint8_t> data = sector_of(0x55);
    drive::Encoder e;
    drive::field(e, layout.data_sync, 0xFB, data.data(), data.size());
    e.byte(0xFF);
    const int k = 3;                                    // the fourth in the order: sector 4
    write(d, in, index + static_cast<std::uint64_t>(drive::data_field_at(layout, k)) * 16 * Mc5305::kCellTicks,
          e.cells);
    const std::vector<std::uint8_t>& now = d.image();
    for (int s = 1; s <= drive::kSectors; ++s) {
        const auto at = static_cast<std::ptrdiff_t>(s - 1) * drive::kSectorBytes;
        const bool same = std::equal(now.begin() + at, now.begin() + at + drive::kSectorBytes,
                                     (s == 4 ? data.begin() : img.begin() + at));
        CHECK_MESSAGE(same, "sector ", s);
    }
    CHECK(std::equal(now.begin() + 5120, now.end(), img.begin() + 5120));     // the rest untouched
}

TEST_CASE("МС 5305: a field with nothing written after its CRC is whole, its last cells 0 or not")
{
    // the cells with no turn of the flux after them count by the time to the write's end
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
        Mc5305 d(image_of(1), layout);
        Lines in = selected();
        const std::uint64_t index = spin_up(d, in);
        drive::Encoder e;
        drive::field(e, layout.data_sync, 0xFB, data.data(), data.size());
        write(d, in, index + static_cast<std::uint64_t>(drive::data_field_at(layout, 3)) * 16 * Mc5305::kCellTicks,
              e.cells);
        CHECK(std::equal(data.begin(), data.end(), d.image().begin() + 3 * drive::kSectorBytes));
    }
    CHECK(ending_0 == 2);
    CHECK(ending_1 == 2);
}

TEST_CASE("МС 5305: a track written from the index (Write Track) puts every sector in the image")
{
    const drive::Layout layout = drive::omega();
    Mc5305 d(image_of(2), layout);
    Lines in = selected();
    in.side = false;                                    // the upper head: side 1
    const std::uint64_t index = spin_up(d, in);
    std::vector<std::uint8_t> sectors;
    for (int s = 0; s < drive::kSectors; ++s) {
        const std::vector<std::uint8_t> one = sector_of(static_cast<std::uint8_t>(s * 17));
        sectors.insert(sectors.end(), one.begin(), one.end());
    }
    std::vector<std::uint8_t> cells = drive::track(layout, 0, 1, sectors.data());
    cells.resize(cells.size() - 64);                    // РЗП off before the index
    write(d, in, index, cells);
    CHECK(std::equal(sectors.begin(), sectors.end(), d.image().begin() + 5120));
}

TEST_CASE("МС 5305: a protected disk takes nothing")
{
    const std::vector<std::uint8_t> img = image_of(2);
    Mc5305 d(img, drive::vvv(), true);
    Lines in = selected();
    const std::uint64_t index = spin_up(d, in);
    std::vector<std::uint8_t> sectors(drive::kSectors * drive::kSectorBytes, 0xE5);
    std::vector<std::uint8_t> cells = drive::track(drive::vvv(), 0, 0, sectors.data());
    cells.resize(cells.size() - 64);
    write(d, in, index, cells);
    CHECK(d.image() == img);
}
