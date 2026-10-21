// КР565РУ5 (4164): a 64K x 1 dynamic RAM driven through its pins - the row
// at -RAS falling, the column at -CAS falling, early and late writes, DO
// driven only while -CAS is low in a read.
#include "chip.h"

#include "Vkr565ru5.h"

#include <random>
#include <vector>

namespace {

using V = Vkr565ru5;

class Ram {
public:
    Ram()
        : chip_({PIN(2), PIN(3), PIN(4), PIN(5), PIN(6), PIN(7), PIN(9), PIN(10), PIN(11),
                 PIN(12), PIN(13), PIN(14), PIN(15)},
                {OE(14)})
    {
        chip_.set(4, true);
        chip_.set(15, true);
        chip_.set(3, true);
        for (int i = 0; i < 4; ++i)
            chip_.tick();
    }

    void address(unsigned a)
    {
        const int pins[8] = {5, 7, 6, 12, 11, 10, 13, 9};   // A0..A7
        for (int k = 0; k < 8; ++k)
            chip_.set(pins[k], ((a >> k) & 1u) != 0);
    }

    // a strobe or input changes, and the master clock runs a few ticks
    void set(int pin, bool level, int ticks = 3)
    {
        chip_.set(pin, level);
        for (int i = 0; i < ticks; ++i)
            chip_.tick();
    }

    void write(unsigned row, unsigned column, bool bit, bool early = true)
    {
        address(row);
        chip_.tick();                   // the row set up before -RAS
        set(4, false);                  // -RAS
        address(column);
        set(2, bit, 1);
        if (early) set(3, false, 1);    // -WE before -CAS
        set(15, false);                 // -CAS
        CHECK(chip_.driven(14) == !early);  // an early write leaves DO alone
        if (!early) set(3, false);      // late: -WE after -CAS
        set(3, true, 1);
        set(15, true, 1);
        set(4, true);
    }

    bool read(unsigned row, unsigned column)
    {
        address(row);
        chip_.tick();
        set(4, false);
        address(column);
        set(15, false);
        REQUIRE(chip_.driven(14));
        const bool bit = chip_.get(14);
        set(15, true);
        CHECK(!chip_.driven(14));
        set(4, true);
        return bit;
    }

    Chip<V>& chip() { return chip_; }

private:
    Chip<V> chip_;
};

}  // namespace

TEST_CASE("КР565РУ5: rows and columns, early and late writes, read back")
{
    Ram ram;
    std::mt19937 rng(565);
    std::vector<unsigned> rows, columns;
    std::vector<bool> bits;
    for (int i = 0; i < 300; ++i) {
        rows.push_back(rng() & 0xFFu);
        columns.push_back(rng() & 0xFFu);
        bits.push_back((rng() & 1u) != 0);
        ram.write(rows.back(), columns.back(), bits.back(), (i & 1) != 0);
    }
    // the last write to a cell wins
    for (int i = 0; i < 300; ++i) {
        bool expected = bits[static_cast<std::size_t>(i)];
        for (int j = i + 1; j < 300; ++j)
            if (rows[static_cast<std::size_t>(j)] == rows[static_cast<std::size_t>(i)] &&
                columns[static_cast<std::size_t>(j)] == columns[static_cast<std::size_t>(i)])
                expected = bits[static_cast<std::size_t>(j)];
        CHECK(ram.read(rows[static_cast<std::size_t>(i)], columns[static_cast<std::size_t>(i)]) == expected);
    }
}

TEST_CASE("КР565РУ5: the row and the column are different halves of the address")
{
    Ram ram;
    ram.write(0x12, 0x34, true);
    ram.write(0x34, 0x12, false);
    CHECK(ram.read(0x12, 0x34));
    CHECK(!ram.read(0x34, 0x12));
}

TEST_CASE("КР565РУ5: -CAS without -RAS does nothing; DO is off while -CAS is high")
{
    Ram ram;
    ram.write(1, 2, true);
    ram.address(2);
    ram.set(15, false);
    CHECK(!ram.chip().driven(14));
    ram.set(15, true);
    CHECK(ram.read(1, 2));
}
