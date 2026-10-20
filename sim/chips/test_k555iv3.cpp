// К555ИВ3 (74LS147): 10-to-4 priority encoder; inputs I1..I9 active low,
// the highest active one wins, the output is its number inverted.
#include "chip.h"

#include "Vk555iv3.h"

TEST_CASE("К555ИВ3: priority encoder")
{
    using V = Vk555iv3;
    Chip<V> chip({PIN(1), PIN(2), PIN(3), PIN(4), PIN(5), PIN(6), PIN(7), PIN(9), PIN(10),
                  PIN(11), PIN(12), PIN(13), PIN(14)});
    // inputs I1..I9; outputs A0..A3
    check_table(chip, {11, 12, 13, 1, 2, 3, 4, 5, 10}, {9, 7, 6, 14},
                [](const std::vector<bool>& v) {
                    unsigned n = 0;
                    for (unsigned k = 1; k <= 9; ++k)
                        if (!v[k - 1])
                            n = k;
                    std::vector<Level> a;
                    for (unsigned b = 0; b < 4; ++b)
                        a.push_back(lv(((n >> b) & 1u) == 0));
                    return a;
                });
}
