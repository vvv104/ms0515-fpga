// К555КП2 (74LS153): two 4-to-1 multiplexers with common select; an
// output is low while its strobe G is high.
#include "chip.h"

#include "Vk555kp2.h"

TEST_CASE("К555КП2: two 4-to-1 multiplexers")
{
    using V = Vk555kp2;
    Chip<V> chip({PIN(1), PIN(2), PIN(3), PIN(4), PIN(5), PIN(6), PIN(7), PIN(9), PIN(10),
                  PIN(11), PIN(12), PIN(13), PIN(14), PIN(15)});
    // inputs S0 S1 1G 1C0..1C3 2G 2C0..2C3; outputs 1Y 2Y
    check_table(chip, {14, 2, 1, 6, 5, 4, 3, 15, 10, 11, 12, 13}, {7, 9},
                [](const std::vector<bool>& v) {
                    const unsigned s = bits(v, 0, 2);
                    return std::vector<Level>{lv(!v[2] && v[3 + s]), lv(!v[7] && v[8 + s])};
                });
}
