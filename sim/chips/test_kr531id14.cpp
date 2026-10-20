// КР531ИД14 (74S139): two 2-to-4 decoders, outputs active low, each
// enabled by its G low.
#include "chip.h"

#include "Vkr531id14.h"

namespace {

std::vector<Level> decode(const std::vector<bool>& v)
{
    std::vector<Level> y;
    for (unsigned k = 0; k < 4; ++k)
        y.push_back(lv(!(!v[2] && bits(v, 0, 2) == k)));
    return y;
}

}  // namespace

TEST_CASE("КР531ИД14: two 2-to-4 decoders")
{
    using V = Vkr531id14;
    Chip<V> chip({PIN(1), PIN(2), PIN(3), PIN(4), PIN(5), PIN(6), PIN(7), PIN(9), PIN(10),
                  PIN(11), PIN(12), PIN(13), PIN(14), PIN(15)});
    check_table(chip, {2, 3, 1}, {4, 5, 6, 7}, decode);
    check_table(chip, {14, 13, 15}, {12, 11, 10, 9}, decode);
}
