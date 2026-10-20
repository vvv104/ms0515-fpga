// К554СА3А (LM311): voltage comparator, open-collector output, against its
// function table.  IN+ at 1 means it is above IN-.
#include "chip.h"

#include "Vk554sa3.h"

TEST_CASE("К554СА3А: comparator, open collector")
{
    using V = Vk554sa3;
    Chip<V> chip({PIN(3), PIN(4), PIN(9)});
    // inputs IN+ IN-; output OUT (released while IN+ is above IN-)
    check_table(chip, {3, 4}, {9},
                [](const std::vector<bool>& v) { return std::vector<Level>{lv(v[0] && !v[1])}; });
}
