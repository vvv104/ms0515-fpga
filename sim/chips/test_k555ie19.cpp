// К555ИЕ19 (74LS393): two 4-bit binary counters, each counting on the
// falling edge of its clock, cleared while its R is high.
#include "chip.h"

#include "Vk555ie19.h"

namespace {

struct Half {
    int c, r, q[4];
};

constexpr Half kHalves[] = {{1, 2, {3, 4, 5, 6}}, {13, 12, {11, 10, 9, 8}}};

}  // namespace

TEST_CASE("К555ИЕ19: random sequences against the function table")
{
    using V = Vk555ie19;
    Chip<V> chip({PIN(1), PIN(2), PIN(3), PIN(4), PIN(5), PIN(6), PIN(8), PIN(9), PIN(10),
                  PIN(11), PIN(12), PIN(13)});
    Dice dice;
    unsigned count[2] = {0, 0};
    bool clock[2] = {true, true};
    for (const Half& h : kHalves) {
        chip.set(h.r, true);
        chip.set(h.c, true);
    }
    chip.tick();
    for (int step = 0; step < 4000; ++step) {
        for (int k = 0; k < 2; ++k) {
            const bool r = dice.one_in(64), c = dice.bit();
            chip.set(kHalves[k].r, r);
            chip.set(kHalves[k].c, c);
            if (r)
                count[k] = 0;
            else if (!c && clock[k])
                count[k] = (count[k] + 1) & 15u;
            clock[k] = c;
        }
        chip.tick();
        for (int k = 0; k < 2; ++k)
            for (int b = 0; b < 4; ++b)
                CHECK(chip.get(kHalves[k].q[b]) == (((count[k] >> b) & 1u) != 0));
    }
}
