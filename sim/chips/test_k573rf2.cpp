// К573РФ2 (2716): every address read through the pins, the outputs driven
// only with CE and OE low - a master tick after the pins say so (the read is
// registered, for the FPGA's block memory; the board's delay lines take that
// tick off tACC, tOE and tDF), as К573РФ4.
#include "chip.h"

#include "Vk573rf2_bench.h"

TEST_CASE("К573РФ2: 2K x 8, read with CE and OE low, a tick after the pins")
{
    using V = Vk573rf2_bench;
    Chip<V> chip({PIN(1), PIN(2), PIN(3), PIN(4), PIN(5), PIN(6), PIN(7), PIN(8), PIN(9), PIN(10),
                  PIN(11), PIN(13), PIN(14), PIN(15), PIN(16), PIN(17), PIN(18), PIN(19), PIN(20),
                  PIN(22), PIN(23)},
                 {OE(9), OE(10), OE(11), OE(13), OE(14), OE(15), OE(16), OE(17)});
    const int address_pins[11] = {8, 7, 6, 5, 4, 3, 2, 1, 23, 22, 19};          // A0..A10
    const int data_pins[8] = {9, 10, 11, 13, 14, 15, 16, 17};                   // O0..O7
    for (unsigned control = 0; control < 4; ++control) {
        const bool ce = control & 1u, oe = (control & 2u) != 0;
        chip.set(18, ce);
        chip.set(20, oe);
        const bool on = !ce && !oe;
        for (unsigned a = 0; a < 2048; a += (on ? 1u : 37u)) {
            for (int k = 0; k < 11; ++k)
                chip.set(address_pins[k], ((a >> k) & 1u) != 0);
            chip.tick();
            const unsigned byte = (a ^ (a >> 8) ^ 0xA5u) & 0xFFu;
            for (int k = 0; k < 8; ++k) {
                CHECK(chip.driven(data_pins[k]) == on);
                if (on)
                    CHECK_MESSAGE(chip.get(data_pins[k]) == (((byte >> k) & 1u) != 0),
                                  "address " << a << " O" << k);
            }
        }
    }
}
