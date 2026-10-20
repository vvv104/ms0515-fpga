// КР1102АП15: two inverting line drivers with a common strobe, against the
// function table inferred from the board (see rtl/chips/kr1102ap15.sv).
#include "chip.h"

#include "Vkr1102ap15.h"

TEST_CASE("КР1102АП15: two inverting line drivers, common strobe")
{
    using V = Vkr1102ap15;
    Chip<V> chip({PIN(1), PIN(2), PIN(3), PIN(6), PIN(7)});
    // inputs E B A; outputs Y YN
    check_table(chip, {1, 2, 3}, {7, 6}, [](const std::vector<bool>& v) {
        return std::vector<Level>{lv(!(v[0] && v[1])), lv(!(v[0] && v[2]))};
    });
}
