// К155ЛН5 (7416): six inverters, open-collector high-voltage outputs, against its function table.
#include "chip.h"

#include "Vk155ln5.h"

TEST_CASE("К155ЛН5: six inverters, open-collector high-voltage outputs")
{
    using V = Vk155ln5;
    Chip<V> chip({PIN(1), PIN(2), PIN(3), PIN(4), PIN(5), PIN(6), PIN(8), PIN(9), PIN(10), PIN(11), PIN(12), PIN(13)});
    check_gates(chip, {{{1}, 2}, {{3}, 4}, {{5}, 6}, {{9}, 8}, {{11}, 10}, {{13}, 12}}, [](const std::vector<bool>& v) { return !v[0]; });
}
