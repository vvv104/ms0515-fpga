// К573РФ4: every address read through the pins, the outputs driven only
// with CE and OE low and PGM high.
#include "chip.h"

#include "Vk573rf4_bench.h"

TEST_CASE("К573РФ4: 8K x 8, read with CE, OE low and PGM high")
{
    using V = Vk573rf4_bench;
    Chip<V> chip({PIN(2), PIN(3), PIN(4), PIN(5), PIN(6), PIN(7), PIN(8), PIN(9), PIN(10),
                  PIN(11), PIN(12), PIN(13), PIN(15), PIN(16), PIN(17), PIN(18), PIN(19),
                  PIN(20), PIN(21), PIN(22), PIN(23), PIN(24), PIN(25), PIN(27)},
                 {OE(11), OE(12), OE(13), OE(15), OE(16), OE(17), OE(18), OE(19)});
    const int address_pins[13] = {10, 9, 8, 7, 6, 5, 4, 3, 25, 24, 21, 23, 2};   // A0..A12
    const int data_pins[8] = {11, 12, 13, 15, 16, 17, 18, 19};                   // D0..D7
    for (unsigned control = 0; control < 8; ++control) {
        const bool ce = control & 1u, oe = (control & 2u) != 0, pgm = (control & 4u) != 0;
        chip.set(20, ce);
        chip.set(22, oe);
        chip.set(27, pgm);
        const bool on = !ce && !oe && pgm;
        for (unsigned a = 0; a < 8192; a += (on ? 1u : 97u)) {
            for (int k = 0; k < 13; ++k)
                chip.set(address_pins[k], ((a >> k) & 1u) != 0);
            chip.eval();
            const unsigned byte = (a ^ (a >> 8) ^ 0x5Au) & 0xFFu;
            for (int k = 0; k < 8; ++k) {
                CHECK(chip.driven(data_pins[k]) == on);
                if (on)
                    CHECK_MESSAGE(chip.get(data_pins[k]) == (((byte >> k) & 1u) != 0),
                                  "address " << a << " D" << k);
            }
        }
    }
}
