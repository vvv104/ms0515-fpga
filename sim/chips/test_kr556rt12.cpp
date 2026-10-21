// КР556РТ12: every address read through the pins as Э3 numbers them, the
// open-collector outputs let go unless both chip enables are low.
#include "chip.h"

#include "Vkr556rt12_bench.h"

TEST_CASE("КР556РТ12: 1024 x 4, open collector, CE1 and CE2 low")
{
    using V = Vkr556rt12_bench;
    Chip<V> chip({PIN(1), PIN(2), PIN(3), PIN(4), PIN(5), PIN(6), PIN(7), PIN(8), PIN(10),
                  PIN(11), PIN(12), PIN(13), PIN(14), PIN(15), PIN(16), PIN(17)});
    const int address_pins[10] = {5, 6, 7, 4, 3, 2, 1, 17, 16, 15};   // A0..A9
    const int outputs[4] = {14, 13, 12, 11};                          // O0..O3
    for (unsigned enables = 0; enables < 4; ++enables) {
        chip.set(8, enables & 1u);
        chip.set(10, (enables & 2u) != 0);
        for (unsigned a = 0; a < 1024; ++a) {
            for (int k = 0; k < 10; ++k)
                chip.set(address_pins[k], ((a >> k) & 1u) != 0);
            chip.eval();
            const unsigned word = enables ? 0xFu : ((a & 15u) ^ ((a >> 4) & 15u) ^ ((a >> 8) << 2)) & 15u;
            for (int k = 0; k < 4; ++k)
                CHECK_MESSAGE(chip.get(outputs[k]) == (((word >> k) & 1u) != 0),
                              "address " << a << " O" << k << " enables " << enables);
        }
    }
}
