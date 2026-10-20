// КР559ИП11 (AM26LS32): four differential line receivers, three-state,
// enabled while EN is high or ENN low, against its function table.  An
// input's level is 1 above the other input of its pair.
#include "chip.h"

#include "Vkr559ip11.h"

TEST_CASE("КР559ИП11: four differential receivers")
{
    using V = Vkr559ip11;
    Chip<V> chip({PIN(1), PIN(2), PIN(3), PIN(4), PIN(5), PIN(6), PIN(7), PIN(9), PIN(10),
                  PIN(11), PIN(12), PIN(13), PIN(14), PIN(15)},
                 {OE(3), OE(5), OE(11), OE(13)});
    // inputs EN ENN 1A 1B 2A 2B 3A 3B 4A 4B; outputs 1Y 2Y 3Y 4Y
    check_table(chip, {4, 12, 2, 1, 6, 7, 10, 9, 14, 15}, {3, 5, 11, 13},
                [](const std::vector<bool>& v) {
                    const bool on = v[0] || !v[1];
                    std::vector<Level> y;
                    for (unsigned k = 0; k < 4; ++k)
                        y.push_back(on ? lv(v[2 + 2 * k] || !v[3 + 2 * k]) : Z);
                    return y;
                });
}
