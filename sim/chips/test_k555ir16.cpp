// К555ИР16 (74LS295): 4-bit shift register with parallel load, clocked
// on the falling edge; MODE high loads A..D, low shifts right (SER into
// QA); three-state outputs driven while OC is high.
#include "chip.h"

#include "Vk555ir16.h"

TEST_CASE("К555ИР16: random sequences against the function table")
{
    using V = Vk555ir16;
    Chip<V> chip({PIN(1), PIN(2), PIN(3), PIN(4), PIN(5), PIN(6), PIN(8), PIN(9), PIN(10),
                  PIN(11), PIN(12), PIN(13)},
                 {OE(13), OE(12), OE(11), OE(10)});
    const int par[4] = {2, 3, 4, 5}, q[4] = {13, 12, 11, 10};   // A..D, QA..QD
    Dice dice;
    bool state[4] = {}, clock = false;
    chip.set(6, true);
    chip.set(9, true);
    chip.tick();
    chip.set(9, false);
    for (int k = 0; k < 4; ++k)
        chip.set(par[k], false);
    chip.tick();
    for (int step = 0; step < 2000; ++step) {
        const bool c = dice.bit(), mode = dice.one_in(3), ser = dice.bit(), oc = !dice.one_in(4);
        chip.set(9, c);
        chip.set(6, mode);
        chip.set(1, ser);
        chip.set(8, oc);
        bool in[4];
        for (int k = 0; k < 4; ++k) {
            in[k] = dice.bit();
            chip.set(par[k], in[k]);
        }
        if (!c && clock) {
            if (mode) {
                for (int k = 0; k < 4; ++k)
                    state[k] = in[k];
            } else {
                for (int k = 3; k > 0; --k)
                    state[k] = state[k - 1];
                state[0] = ser;
            }
        }
        clock = c;
        chip.tick();
        for (int k = 0; k < 4; ++k) {
            CHECK(chip.driven(q[k]) == oc);
            if (oc)
                CHECK(chip.get(q[k]) == state[k]);
        }
    }
}
