// К555КП11 (74LS257): four 2-to-1 multiplexers with three-state outputs;
// SEL low takes A, high takes B; OE high lets the outputs go.
#include "chip.h"

#include "Vk555kp11.h"

TEST_CASE("К555КП11: four 2-to-1 multiplexers, three-state")
{
    using V = Vk555kp11;
    Chip<V> chip({PIN(1), PIN(2), PIN(3), PIN(4), PIN(5), PIN(6), PIN(7), PIN(9), PIN(10),
                  PIN(11), PIN(12), PIN(13), PIN(14), PIN(15)},
                 {OE(4), OE(7), OE(9), OE(12)});
    // inputs SEL OE 1A 1B 2A 2B 3A 3B 4A 4B; outputs 1Y 2Y 3Y 4Y
    check_table(chip, {1, 15, 2, 3, 5, 6, 11, 10, 14, 13}, {4, 7, 9, 12},
                [](const std::vector<bool>& v) {
                    std::vector<Level> y;
                    for (unsigned k = 0; k < 4; ++k)
                        y.push_back(v[1] ? Z : lv(v[2 + 2 * k + (v[0] ? 1 : 0)]));
                    return y;
                });
}
