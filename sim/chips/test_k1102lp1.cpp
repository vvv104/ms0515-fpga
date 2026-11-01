// К1102ЛП1: the МС 7004's line receiver, as its board draws it - pin 7 in,
// pin 2 out, inverted (the line's negative mark a logic 1).
#include "chip.h"

#include "Vk1102lp1.h"

TEST_CASE("К1102ЛП1: the receiver inverts the line")
{
    using V = Vk1102lp1;
    Chip<V> chip({PIN(2), PIN(7)});
    check_gates(chip, {{{7}, 2}}, [](const std::vector<bool>& v) { return !v[0]; });
}
