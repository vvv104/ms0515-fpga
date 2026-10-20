// К555ТЛ2 (74LS14): six Schmitt-trigger inverters (the hysteresis is analog; digitally an inverter), against its function table.
#include "chip.h"

#include "Vk555tl2.h"

TEST_CASE("К555ТЛ2: six Schmitt-trigger inverters (the hysteresis is analog; digitally an inverter)")
{
    using V = Vk555tl2;
    Chip<V> chip({PIN(1), PIN(2), PIN(3), PIN(4), PIN(5), PIN(6), PIN(8), PIN(9), PIN(10), PIN(11), PIN(12), PIN(13)});
    check_gates(chip, {{{1}, 2}, {{3}, 4}, {{5}, 6}, {{9}, 8}, {{11}, 10}, {{13}, 12}}, [](const std::vector<bool>& v) { return !v[0]; });
}
