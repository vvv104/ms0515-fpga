// К555ИР23 (74LS374): eight D flip-flops, rising clock, three-state
// outputs driven while OE is low.
#include "chip.h"

#include "Vk555ir23.h"

TEST_CASE("К555ИР23: random sequences against the function table")
{
    using V = Vk555ir23;
    Chip<V> chip({PIN(1), PIN(2), PIN(3), PIN(4), PIN(5), PIN(6), PIN(7), PIN(8), PIN(9),
                  PIN(11), PIN(12), PIN(13), PIN(14), PIN(15), PIN(16), PIN(17), PIN(18),
                  PIN(19)},
                 {OE(2), OE(5), OE(6), OE(9), OE(12), OE(15), OE(16), OE(19)});
    const int d[8] = {3, 4, 7, 8, 13, 14, 17, 18}, q[8] = {2, 5, 6, 9, 12, 15, 16, 19};
    Dice dice;
    bool state[8] = {}, clock = false;
    chip.set(11, false);
    chip.tick();
    for (int k = 0; k < 8; ++k)
        state[k] = chip.get(q[k]);
    for (int step = 0; step < 2000; ++step) {
        const bool c = dice.bit(), oe_n = dice.one_in(4);
        chip.set(11, c);
        chip.set(1, oe_n);
        bool in[8];
        for (int k = 0; k < 8; ++k) {
            in[k] = dice.bit();
            chip.set(d[k], in[k]);
            if (c && !clock)
                state[k] = in[k];
        }
        clock = c;
        chip.tick();
        for (int k = 0; k < 8; ++k) {
            CHECK(chip.driven(q[k]) == !oe_n);
            if (!oe_n)
                CHECK(chip.get(q[k]) == state[k]);
        }
    }
}
