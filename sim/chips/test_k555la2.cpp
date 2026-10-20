// К555ЛА2 (74LS30): an 8-input NAND gate, against its function table.
#include "chip.h"

#include "Vk555la2.h"

TEST_CASE("К555ЛА2: an 8-input NAND gate")
{
    using V = Vk555la2;
    Chip<V> chip({PIN(1), PIN(2), PIN(3), PIN(4), PIN(5), PIN(6), PIN(8), PIN(11), PIN(12)});
    check_gates(chip, {{{1, 2, 3, 4, 5, 6, 11, 12}, 8}}, [](const std::vector<bool>& v) { return !all_of(v); });
}
