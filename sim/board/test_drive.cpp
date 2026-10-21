// The floppy drive's track: a side's sectors laid out by a formatter's
// layout, MFM-encoded into the flux cells a revolution holds, and read
// back as the controller would - the marks found by their missing clocks,
// the IDs and the data checked by their CRCs.
#include <doctest/doctest.h>

#include "drive.h"

#include <cstdint>
#include <vector>

namespace {

// The cells back into bytes from a sync onwards: the decoder a
// separator and the FD1793 make together, as plain as a test needs.
struct Reader {
    const std::vector<std::uint8_t>& cells;
    std::size_t at = 0;

    // the next A1 with its missing clock (0x4489), from at on
    bool find_sync()
    {
        std::uint16_t window = 0;
        for (; at < cells.size(); ++at) {
            window = static_cast<std::uint16_t>(window << 1 | cells[at]);
            if (window == 0x4489) {
                ++at;
                return true;
            }
        }
        return false;
    }

    std::uint8_t byte()
    {
        std::uint8_t b = 0;
        for (int i = 0; i < 8; ++i, at += 2)
            b = static_cast<std::uint8_t>(b << 1 | cells[at + 1]);     // the data cell of each pair
        return b;
    }
};

std::vector<std::uint8_t> side_of(std::uint8_t seed)
{
    std::vector<std::uint8_t> s(drive::kSectors * drive::kSectorBytes);
    for (std::size_t i = 0; i < s.size(); ++i)
        s[i] = static_cast<std::uint8_t>(i * 7 + seed);
    return s;
}

}  // namespace

TEST_CASE("drive: CRC-CCITT of an ID field as the FD1793 computes it")
{
    // A1 A1 A1 FE 00 00 01 02: the ID of track 0, side 0, sector 1, 512 bytes
    const std::uint8_t id[] = {0xA1, 0xA1, 0xA1, 0xFE, 0x00, 0x00, 0x01, 0x02};
    CHECK(drive::crc(id, sizeof id) == 0xCA6F);
}

TEST_CASE("drive: a track is a revolution of cells and reads back sector by sector")
{
    const drive::Layout layout;                        // the default: the datasheet's minimum
    const std::vector<std::uint8_t> sectors = side_of(3);
    const std::vector<std::uint8_t> cells = drive::track(layout, 5, 1, sectors.data());
    CHECK(cells.size() == drive::kCellsPerTurn);

    Reader r{cells};
    int found = 0;
    for (int k = 0; k < drive::kSectors; ++k) {
        REQUIRE(r.find_sync());
        r.at += 2 * 16;                                // the other two A1
        std::uint8_t id[8] = {0xA1, 0xA1, 0xA1};
        for (int i = 3; i < 8; ++i) id[i] = r.byte();
        CHECK(id[3] == 0xFE);
        CHECK(id[4] == 5);                             // C, the track
        CHECK(id[5] == 1);                             // H, the side
        CHECK(id[7] == 2);                             // N, 512 bytes
        const std::uint16_t want = static_cast<std::uint16_t>(r.byte() << 8 | r.byte());
        CHECK(drive::crc(id, 8) == want);
        const int sector = id[6];
        REQUIRE(sector >= 1);
        REQUIRE(sector <= drive::kSectors);
        REQUIRE(r.find_sync());
        r.at += 2 * 16;
        std::vector<std::uint8_t> field = {0xA1, 0xA1, 0xA1, r.byte()};
        CHECK(field[3] == 0xFB);
        for (int i = 0; i < drive::kSectorBytes; ++i) field.push_back(r.byte());
        const std::uint16_t dcrc = static_cast<std::uint16_t>(r.byte() << 8 | r.byte());
        CHECK(drive::crc(field.data(), field.size()) == dcrc);
        const std::uint8_t* want_data = sectors.data() + (sector - 1) * drive::kSectorBytes;
        CHECK(std::equal(field.begin() + 4, field.end(), want_data));
        ++found;
    }
    CHECK(found == drive::kSectors);
}

TEST_CASE("drive: the three original formatters' layouts fit a revolution")
{
    // the bytes before Gap IV, as the formatters' buffers hold them
    CHECK(drive::bytes_before_gap4(drive::omega()) == 6245);
    CHECK(drive::bytes_before_gap4(drive::vvv()) == 6186);
    CHECK(drive::bytes_before_gap4(drive::fdz()) == 6184);
    const std::vector<std::uint8_t> sectors = side_of(9);
    for (const drive::Layout& l : {drive::omega(), drive::vvv(), drive::fdz()})
        CHECK(drive::track(l, 79, 1, sectors.data()).size() == drive::kCellsPerTurn);
}

TEST_CASE("drive: Omega's formatter writes H 0 on the upper side too")
{
    const std::vector<std::uint8_t> sectors = side_of(1);
    for (const auto& [l, h] : {std::pair{drive::omega(), 0}, std::pair{drive::vvv(), 1}}) {
        const std::vector<std::uint8_t> cells = drive::track(l, 3, 1, sectors.data());
        Reader r{cells};
        // past the index mark's C2s (Omega's and vvv's), to the first ID
        do {
            REQUIRE(r.find_sync());
            r.at += 2 * 16;
        } while (r.byte() != 0xFE);
        CHECK(r.byte() == 3);                         // C
        CHECK(r.byte() == h);                         // H
    }
}

TEST_CASE("drive: the sectors go round in the layout's order")
{
    drive::Layout layout;
    layout.order = {1, 6, 2, 7, 3, 8, 4, 9, 5, 10};
    const std::vector<std::uint8_t> sectors = side_of(0);
    const std::vector<std::uint8_t> cells = drive::track(layout, 0, 0, sectors.data());
    Reader r{cells};
    for (int k = 0; k < drive::kSectors; ++k) {
        REQUIRE(r.find_sync());
        r.at += 2 * 16;
        r.byte();                                      // FE
        r.byte();                                      // C
        r.byte();                                      // H
        CHECK(r.byte() == layout.order[static_cast<std::size_t>(k)]);
        REQUIRE(r.find_sync());                        // the data field's
        r.at += 2 * 16 + 16 * (1 + drive::kSectorBytes + 2);
    }
}
