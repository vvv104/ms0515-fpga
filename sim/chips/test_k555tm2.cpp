// К555ТМ2 (74LS74): two D flip-flops, rising edge, asynchronous set and
// reset (active low).  КР531ТМ2 and КР1533ТМ2 are the same function.
#include "chip.h"

#include "Vk555tm2.h"

namespace {

struct Section {
    int r, d, c, s, q, qn;
};

constexpr Section kSections[] = {{1, 2, 3, 4, 5, 6}, {13, 12, 11, 10, 9, 8}};

}  // namespace

TEST_CASE("К555ТМ2: set, reset, both, clock")
{
    using V = Vk555tm2;
    Chip<V> chip({PIN(1), PIN(2), PIN(3), PIN(4), PIN(5), PIN(6), PIN(8), PIN(9), PIN(10),
                  PIN(11), PIN(12), PIN(13)});
    for (const Section& x : kSections) {
        chip.set(x.c, false);
        chip.set(x.s, false);
        chip.set(x.r, true);
        chip.eval();                       // asynchronous: before any master edge
        CHECK(chip.get(x.q));
        CHECK(!chip.get(x.qn));
        chip.set(x.s, true);
        chip.set(x.r, false);
        chip.eval();
        CHECK(!chip.get(x.q));
        CHECK(chip.get(x.qn));
        chip.set(x.s, false);
        chip.eval();
        CHECK(chip.get(x.q));              // both active: both outputs high
        CHECK(chip.get(x.qn));
        chip.set(x.s, true);
        chip.set(x.r, true);
        chip.set(x.d, true);
        chip.tick();
        chip.set(x.c, true);
        chip.tick();
        CHECK(chip.get(x.q));
        chip.set(x.d, false);
        chip.tick();                       // no edge: holds
        CHECK(chip.get(x.q));
        chip.set(x.c, false);
        chip.tick();
        CHECK(chip.get(x.q));
        chip.set(x.c, true);
        chip.tick();
        CHECK(!chip.get(x.q));
        CHECK(chip.get(x.qn));
    }
}

TEST_CASE("К555ТМ2: random sequences against the function table")
{
    using V = Vk555tm2;
    Chip<V> chip({PIN(1), PIN(2), PIN(3), PIN(4), PIN(5), PIN(6), PIN(8), PIN(9), PIN(10),
                  PIN(11), PIN(12), PIN(13)});
    Dice dice;
    bool state[2] = {false, false};
    bool clock[2] = {false, false};
    for (const Section& x : kSections) {
        chip.set(x.s, true);
        chip.set(x.r, false);
        chip.set(x.c, false);
    }
    chip.tick();
    for (int step = 0; step < 2000; ++step) {
        for (int k = 0; k < 2; ++k) {
            const Section& x = kSections[k];
            const bool s = !dice.one_in(8), r = s ? !dice.one_in(8) : true;
            const bool d = dice.bit(), c = dice.bit();
            chip.set(x.s, s);
            chip.set(x.r, r);
            chip.set(x.d, d);
            chip.set(x.c, c);
            if (!s)
                state[k] = true;
            else if (!r)
                state[k] = false;
            else if (c && !clock[k])
                state[k] = d;
            clock[k] = c;
        }
        chip.tick();
        for (int k = 0; k < 2; ++k) {
            CHECK(chip.get(kSections[k].q) == state[k]);
            CHECK(chip.get(kSections[k].qn) == !state[k]);
        }
    }
}
