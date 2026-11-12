// КР565РУ7 (41256): a 256K x 1 dynamic RAM driven through its pins - nine
// row and nine column bits (A8 on pin 1), early and late writes, DO driven
// only while -CAS is low in a read.
#include "chip.h"

#include "Vkr565ru7.h"

#include <random>
#include <vector>

namespace {

using V = Vkr565ru7;

class Ram {
public:
    Ram()
        : chip_({PIN(1), PIN(2), PIN(3), PIN(4), PIN(5), PIN(6), PIN(7), PIN(9), PIN(10), PIN(11),
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
        const int pins[9] = {5, 7, 6, 12, 11, 10, 13, 9, 1};   // A0..A8
        for (int k = 0; k < 9; ++k)
            chip_.set(pins[k], ((a >> k) & 1u) != 0);
    }

    void set(int pin, bool level, int ticks = 3)
    {
        chip_.set(pin, level);
        for (int i = 0; i < ticks; ++i)
            chip_.tick();
    }

    void write(unsigned row, unsigned column, bool bit, bool early = true)
    {
        address(row);
        chip_.tick();
        set(4, false);
        address(column);
        set(2, bit, 1);
        if (early) set(3, false, 1);
        set(15, false);
        CHECK(chip_.driven(14) == !early);
        if (!early) set(3, false);
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

TEST_CASE("КР565РУ7: rows and columns of nine bits, early and late writes, read back")
{
    Ram ram;
    std::mt19937 rng(41256);
    std::vector<unsigned> rows, columns;
    std::vector<bool> bits;
    for (int i = 0; i < 300; ++i) {
        rows.push_back(rng() & 0x1FFu);
        columns.push_back(rng() & 0x1FFu);
        bits.push_back((rng() & 1u) != 0);
        ram.write(rows.back(), columns.back(), bits.back(), (i & 1) != 0);
    }
    for (std::size_t i = 0; i < 300; ++i) {
        bool expected = bits[i];
        for (std::size_t j = i + 1; j < 300; ++j)
            if (rows[j] == rows[i] && columns[j] == columns[i])
                expected = bits[j];
        CHECK(ram.read(rows[i], columns[i]) == expected);
    }
}

TEST_CASE("КР565РУ7: A8 tells cells apart in the row and in the column")
{
    Ram ram;
    ram.write(0x012, 0x034, true);
    ram.write(0x112, 0x034, false);
    ram.write(0x012, 0x134, false);
    CHECK(ram.read(0x012, 0x034));
    CHECK(!ram.read(0x112, 0x034));
    CHECK(!ram.read(0x012, 0x134));
}

TEST_CASE("КР565РУ7: DO is off while -CAS is high, and -CAS without -RAS does nothing")
{
    Ram ram;
    ram.write(1, 2, true);
    ram.address(2);
    ram.set(15, false);
    CHECK(!ram.chip().driven(14));
    ram.set(15, true);
    CHECK(ram.read(1, 2));
}
