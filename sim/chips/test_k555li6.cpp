// К555ЛИ6 (74LS21): two 4-input AND gates, against its function table.
#include "chip.h"

#include "Vk555li6.h"

TEST_CASE("К555ЛИ6: two 4-input AND gates")
{
    using V = Vk555li6;
    Chip<V> chip({PIN(1), PIN(2), PIN(4), PIN(5), PIN(6), PIN(8), PIN(9), PIN(10), PIN(12), PIN(13)});
    check_gates(chip, {{{1, 2, 4, 5}, 6}, {{9, 10, 12, 13}, 8}}, [](const std::vector<bool>& v) { return all_of(v); });
}
