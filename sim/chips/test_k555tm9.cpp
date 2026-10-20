// К555ТМ9 (74LS174): six D flip-flops, common rising clock, asynchronous
// clear (active low).
#include "chip.h"

#include "Vk555tm9.h"

TEST_CASE("К555ТМ9: random sequences against the function table")
{
    using V = Vk555tm9;
    Chip<V> chip({PIN(1), PIN(2), PIN(3), PIN(4), PIN(5), PIN(6), PIN(7), PIN(9), PIN(10),
                  PIN(11), PIN(12), PIN(13), PIN(14), PIN(15)});
    const int d[6] = {3, 4, 6, 11, 13, 14}, q[6] = {2, 5, 7, 10, 12, 15};
    Dice dice;
    bool state[6] = {}, clock = false;
    chip.set(1, false);
    chip.set(9, false);
    chip.tick();
    for (int step = 0; step < 2000; ++step) {
        const bool clr = dice.one_in(16), c = dice.bit();
        chip.set(1, !clr);
        chip.set(9, c);
        bool in[6];
        for (int k = 0; k < 6; ++k) {
            in[k] = dice.bit();
            chip.set(d[k], in[k]);
        }
        for (int k = 0; k < 6; ++k)
            if (clr)
                state[k] = false;
            else if (c && !clock)
                state[k] = in[k];
        clock = c;
        chip.tick();
        for (int k = 0; k < 6; ++k)
            CHECK(chip.get(q[k]) == state[k]);
    }
}
