// К555КП15 (74LS251): 8-to-1 multiplexer with three-state outputs Y and
// W = not Y; OE high lets both go.
#include "chip.h"

#include "Vk555kp15.h"

TEST_CASE("К555КП15: 8-to-1 multiplexer, three-state")
{
    using V = Vk555kp15;
    Chip<V> chip({PIN(1), PIN(2), PIN(3), PIN(4), PIN(5), PIN(6), PIN(7), PIN(9), PIN(10),
                  PIN(11), PIN(12), PIN(13), PIN(14), PIN(15)},
                 {OE(5), OE(6)});
    // inputs S0 S1 S2 OE D0..D7; outputs Y W
    check_table(chip, {11, 10, 9, 7, 4, 3, 2, 1, 15, 14, 13, 12}, {5, 6},
                [](const std::vector<bool>& v) {
                    if (v[3])
                        return std::vector<Level>{Z, Z};
                    const bool d = v[4 + bits(v, 0, 3)];
                    return std::vector<Level>{lv(d), lv(!d)};
                });
}
