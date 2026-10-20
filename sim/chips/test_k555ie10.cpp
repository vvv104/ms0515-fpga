// К555ИЕ10 (74LS161): 4-bit binary counter, rising clock, synchronous load
// (LOAD low), asynchronous clear (CLR low), counts while ENP and ENT are
// high; RCO = ENT and the count at 15.
#include "chip.h"

#include "Vk555ie10.h"

TEST_CASE("К555ИЕ10: random sequences against the function table")
{
    using V = Vk555ie10;
    Chip<V> chip({PIN(1), PIN(2), PIN(3), PIN(4), PIN(5), PIN(6), PIN(7), PIN(9), PIN(10),
                  PIN(11), PIN(12), PIN(13), PIN(14), PIN(15)});
    const int data[4] = {3, 4, 5, 6}, q[4] = {14, 13, 12, 11};   // A..D, QA..QD
    Dice dice;
    unsigned count = 0;
    bool clock = false;
    chip.set(1, false);
    chip.set(2, false);
    chip.tick();
    for (int step = 0; step < 4000; ++step) {
        const bool clr = dice.one_in(64), load = dice.one_in(16), c = dice.bit();
        const bool enp = !dice.one_in(4), ent = !dice.one_in(4);
        unsigned in = 0;
        for (int k = 0; k < 4; ++k) {
            const bool b = dice.bit();
            chip.set(data[k], b);
            in |= (b ? 1u : 0u) << k;
        }
        chip.set(1, !clr);
        chip.set(9, !load);
        chip.set(2, c);
        chip.set(7, enp);
        chip.set(10, ent);
        if (clr)
            count = 0;
        else if (c && !clock)
            count = load ? in : ((enp && ent) ? (count + 1) & 15u : count);
        clock = c;
        chip.tick();
        for (int k = 0; k < 4; ++k)
            CHECK(chip.get(q[k]) == (((count >> k) & 1u) != 0));
        CHECK(chip.get(15) == (ent && count == 15));
    }
}
