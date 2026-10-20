// К555ЛН2 (74LS05): six inverters, open-collector outputs, against its function table.
#include "chip.h"

#include "Vk555ln2.h"

TEST_CASE("К555ЛН2: six inverters, open-collector outputs")
{
    using V = Vk555ln2;
    Chip<V> chip({PIN(1), PIN(2), PIN(3), PIN(4), PIN(5), PIN(6), PIN(8), PIN(9), PIN(10), PIN(11), PIN(12), PIN(13)});
    check_gates(chip, {{{1}, 2}, {{3}, 4}, {{5}, 6}, {{9}, 8}, {{11}, 10}, {{13}, 12}}, [](const std::vector<bool>& v) { return !v[0]; });
}
