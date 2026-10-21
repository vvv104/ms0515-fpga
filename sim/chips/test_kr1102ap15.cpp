// КР1102АП15 (uA9636): two inverting RS-232 line drivers, 1A (pin 2) to 1Y
// (pin 7) and 2A (pin 3) to 2Y (pin 6) - eandc К1102АП15 p.1, TI SLLS110B
// p.1.  Pin 1 takes the slew-rate resistor: no logic.
#include "chip.h"

#include "Vkr1102ap15.h"

TEST_CASE("КР1102АП15: two inverting line drivers")
{
    using V = Vkr1102ap15;
    Chip<V> chip({PIN(2), PIN(3), PIN(6), PIN(7)});
    check_table(chip, {2, 3}, {7, 6}, [](const std::vector<bool>& v) {
        return std::vector<Level>{lv(!v[0]), lv(!v[1])};
    });
}
