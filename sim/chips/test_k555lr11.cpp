// К555ЛР11 (74LS51): two AND-OR-INVERT gates, against its function table.
#include "chip.h"

#include "Vk555lr11.h"

TEST_CASE("К555ЛР11: 1Y = not(1A.1B.1C + 1D.1E.1F), 2Y = not(2A.2B + 2C.2D)")
{
    using V = Vk555lr11;
    Chip<V> chip({PIN(1), PIN(2), PIN(3), PIN(4), PIN(5), PIN(6), PIN(8), PIN(9), PIN(10),
                  PIN(11), PIN(12), PIN(13)});
    check_gates(chip, {{{1, 12, 13, 9, 10, 11}, 8}}, [](const std::vector<bool>& v) {
        return !((v[0] && v[1] && v[2]) || (v[3] && v[4] && v[5]));
    });
    check_gates(chip, {{{2, 3, 4, 5}, 6}}, [](const std::vector<bool>& v) {
        return !((v[0] && v[1]) || (v[2] && v[3]));
    });
}
