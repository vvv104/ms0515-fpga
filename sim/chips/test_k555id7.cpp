// К555ИД7 (74LS138): 3-to-8 decoder, outputs active low, enabled by
// G1 high and G2A, G2B low.
#include "chip.h"

#include "Vk555id7.h"

TEST_CASE("К555ИД7: 3-to-8 decoder")
{
    using V = Vk555id7;
    Chip<V> chip({PIN(1), PIN(2), PIN(3), PIN(4), PIN(5), PIN(6), PIN(7), PIN(9), PIN(10),
                  PIN(11), PIN(12), PIN(13), PIN(14), PIN(15)});
    // inputs A B C G2A G2B G1; outputs Y0..Y7
    check_table(chip, {1, 2, 3, 4, 5, 6}, {15, 14, 13, 12, 11, 10, 9, 7},
                [](const std::vector<bool>& v) {
                    const bool on = v[5] && !v[3] && !v[4];
                    std::vector<Level> y;
                    for (unsigned k = 0; k < 8; ++k)
                        y.push_back(lv(!(on && bits(v, 0, 3) == k)));
                    return y;
                });
}
