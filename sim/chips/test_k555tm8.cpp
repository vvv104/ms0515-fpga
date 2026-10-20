// К555ТМ8 (74LS175): four D flip-flops with Q and not-Q, common rising
// clock, asynchronous clear (active low).
#include "chip.h"

#include "Vk555tm8.h"

TEST_CASE("К555ТМ8: random sequences against the function table")
{
    using V = Vk555tm8;
    Chip<V> chip({PIN(1), PIN(2), PIN(3), PIN(4), PIN(5), PIN(6), PIN(7), PIN(9), PIN(10),
                  PIN(11), PIN(12), PIN(13), PIN(14), PIN(15)});
    const int d[4] = {4, 5, 12, 13}, q[4] = {2, 7, 10, 15}, qn[4] = {3, 6, 11, 14};
    Dice dice;
    bool state[4] = {}, clock = false;
    chip.set(1, false);
    chip.set(9, false);
    chip.tick();
    for (int step = 0; step < 2000; ++step) {
        const bool clr = dice.one_in(16), c = dice.bit();
        chip.set(1, !clr);
        chip.set(9, c);
        bool in[4];
        for (int k = 0; k < 4; ++k) {
            in[k] = dice.bit();
            chip.set(d[k], in[k]);
        }
        chip.eval();
        if (clr) {
            for (int k = 0; k < 4; ++k)
                CHECK(!chip.get(q[k]));     // clear acts before the master edge
        }
        for (int k = 0; k < 4; ++k)
            if (clr)
                state[k] = false;
            else if (c && !clock)
                state[k] = in[k];
        clock = c;
        chip.tick();
        for (int k = 0; k < 4; ++k) {
            CHECK(chip.get(q[k]) == state[k]);
            CHECK(chip.get(qn[k]) == !state[k]);
        }
    }
}
