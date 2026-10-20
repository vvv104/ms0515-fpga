// К555ИР22 (74LS373): eight transparent latches, three-state outputs;
// C high lets D through, C low holds; OE low drives the outputs.
#include "chip.h"

#include "Vk555ir22.h"

TEST_CASE("К555ИР22: random sequences against the function table")
{
    using V = Vk555ir22;
    Chip<V> chip({PIN(1), PIN(2), PIN(3), PIN(4), PIN(5), PIN(6), PIN(7), PIN(8), PIN(9),
                  PIN(11), PIN(12), PIN(13), PIN(14), PIN(15), PIN(16), PIN(17), PIN(18),
                  PIN(19)},
                 {OE(2), OE(5), OE(6), OE(9), OE(12), OE(15), OE(16), OE(19)});
    const int d[8] = {3, 4, 7, 8, 13, 14, 17, 18}, q[8] = {2, 5, 6, 9, 12, 15, 16, 19};
    Dice dice;
    bool held[8] = {};
    for (int step = 0; step < 2000; ++step) {
        const bool c = dice.bit(), oe_n = dice.one_in(4);
        chip.set(11, c);
        chip.set(1, oe_n);
        bool in[8];
        for (int k = 0; k < 8; ++k) {
            in[k] = dice.bit();
            chip.set(d[k], in[k]);
        }
        chip.tick();                       // transparent: D shows at the next master edge
        for (int k = 0; k < 8; ++k) {
            if (c)
                held[k] = in[k];
            CHECK(chip.driven(q[k]) == !oe_n);
            if (!oe_n)
                CHECK(chip.get(q[k]) == held[k]);
        }
    }
}
