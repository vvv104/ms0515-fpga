// К555АП5 (74LS244): two groups of four buffers, three-state, each group
// enabled by its G low.
#include "chip.h"

#include "Vk555ap5.h"

TEST_CASE("К555АП5: three-state buffers")
{
    using V = Vk555ap5;
    Chip<V> chip({PIN(1), PIN(2), PIN(3), PIN(4), PIN(5), PIN(6), PIN(7), PIN(8), PIN(9),
                  PIN(11), PIN(12), PIN(13), PIN(14), PIN(15), PIN(16), PIN(17), PIN(18),
                  PIN(19)},
                 {OE(18), OE(16), OE(14), OE(12), OE(9), OE(7), OE(5), OE(3)});
    // inputs 1G 1A1..1A4 2G 2A1..2A4; outputs 1Y1..1Y4 2Y1..2Y4
    check_table(chip, {1, 2, 4, 6, 8, 19, 11, 13, 15, 17}, {18, 16, 14, 12, 9, 7, 5, 3},
                [](const std::vector<bool>& v) {
                    std::vector<Level> y;
                    for (unsigned k = 0; k < 4; ++k)
                        y.push_back(v[0] ? Z : lv(v[1 + k]));
                    for (unsigned k = 0; k < 4; ++k)
                        y.push_back(v[5] ? Z : lv(v[6 + k]));
                    return y;
                });
}
