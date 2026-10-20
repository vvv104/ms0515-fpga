// К555ЛЛ1 (74LS32): four 2-input OR gates, against its function table.
#include "chip.h"

#include "Vk555ll1.h"

TEST_CASE("К555ЛЛ1: four 2-input OR gates")
{
    using V = Vk555ll1;
    Chip<V> chip({PIN(1), PIN(2), PIN(3), PIN(4), PIN(5), PIN(6), PIN(8), PIN(9), PIN(10), PIN(11), PIN(12), PIN(13)});
    check_gates(chip, {{{1, 2}, 3}, {{4, 5}, 6}, {{9, 10}, 8}, {{12, 13}, 11}}, [](const std::vector<bool>& v) { return any_of(v); });
}
