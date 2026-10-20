// КР531ЛЕ1 (74S02): four 2-input NOR gates, against its function table.
#include "chip.h"

#include "Vkr531le1.h"

TEST_CASE("КР531ЛЕ1: four 2-input NOR gates")
{
    using V = Vkr531le1;
    Chip<V> chip({PIN(1), PIN(2), PIN(3), PIN(4), PIN(5), PIN(6), PIN(8), PIN(9), PIN(10), PIN(11), PIN(12), PIN(13)});
    check_gates(chip, {{{2, 3}, 1}, {{5, 6}, 4}, {{8, 9}, 10}, {{11, 12}, 13}}, [](const std::vector<bool>& v) { return !any_of(v); });
}
