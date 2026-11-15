// К555ИР16 (74LS295): 4-bit shift register with parallel load, clocked
// on the falling edge; MODE high loads A..D, low shifts right (SER into
// QA); three-state outputs driven while OC is high.  Every input - MODE,
// A..D and SER - counts as it stood SETUP ticks before the edge (the
// sheet: setup 20 ns for all of them, hold 0 for MODE and 10 for data);
// these cases run the default, 3 ticks - the sheet's 20 ns: an input that
// changes with the clock, or up to two ticks before it, is not yet seen.
// The board gives each corner its own SETUP (tools/board.py, SETUP_NS).
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
    bool mode_was[3] = {true, true, true}, ser_was[3] = {}, in_was[3][4] = {};   // [0] the newest
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
            if (mode_was[2]) {
                for (int k = 0; k < 4; ++k)
                    state[k] = in_was[2][k];
            } else {
                for (int k = 3; k > 0; --k)
                    state[k] = state[k - 1];
                state[0] = ser_was[2];
            }
        }
        clock = c;
        for (int h = 2; h > 0; --h) {
            mode_was[h] = mode_was[h - 1];
            ser_was[h] = ser_was[h - 1];
            for (int k = 0; k < 4; ++k)
                in_was[h][k] = in_was[h - 1][k];
        }
        mode_was[0] = mode;
        ser_was[0] = ser;
        for (int k = 0; k < 4; ++k)
            in_was[0][k] = in[k];
        chip.tick();
        for (int k = 0; k < 4; ++k) {
            CHECK(chip.driven(q[k]) == oc);
            if (oc)
                CHECK(chip.get(q[k]) == state[k]);
        }
    }
}

// D70 coming on at the end of the border, as Э3 has it: the clock and MODE
// both leave the floating high in the same tick - the edge loads.
TEST_CASE("К555ИР16: MODE falling with the clock loads")
{
    using V = Vk555ir16;
    Chip<V> chip({PIN(1), PIN(2), PIN(3), PIN(4), PIN(5), PIN(6), PIN(8), PIN(9), PIN(10),
                  PIN(11), PIN(12), PIN(13)},
                 {OE(13), OE(12), OE(11), OE(10)});
    chip.set(8, true);
    chip.set(1, false);
    chip.set(6, false);
    for (const int p : {2, 3, 4, 5})
        chip.set(p, false);
    chip.set(9, true);
    chip.tick();
    chip.set(9, false);
    chip.tick();   // QA..QD all 0: shifted in from SER
    chip.set(9, true);
    chip.set(6, true);
    for (const int p : {2, 3, 4, 5})
        chip.set(p, true);
    chip.tick();
    chip.tick();
    chip.tick();   // MODE high through the 20 ns setup
    chip.set(9, false);
    chip.set(6, false);
    chip.tick();
    for (const int q : {13, 12, 11, 10})
        CHECK(chip.get(q));
}

// D70 coming on at the left edge of a 640 x 200 line with the chips as
// fitted: MODE leaves the floating high one tick (8 ns) before the clock
// falls - inside the 20 ns setup, so the edge still loads.
TEST_CASE("К555ИР16: MODE falling two ticks before the clock still loads")
{
    using V = Vk555ir16;
    Chip<V> chip({PIN(1), PIN(2), PIN(3), PIN(4), PIN(5), PIN(6), PIN(8), PIN(9), PIN(10),
                  PIN(11), PIN(12), PIN(13)},
                 {OE(13), OE(12), OE(11), OE(10)});
    chip.set(8, true);
    chip.set(1, false);
    chip.set(6, false);
    for (const int p : {2, 3, 4, 5})
        chip.set(p, false);
    chip.set(9, true);
    chip.tick();
    chip.set(9, false);
    chip.tick();   // QA..QD all 0: shifted in from SER
    chip.set(9, true);
    chip.set(6, true);
    for (const int p : {2, 3, 4, 5})
        chip.set(p, true);
    chip.tick();
    chip.tick();
    chip.set(6, false);
    chip.tick();
    chip.tick();
    chip.set(9, false);
    chip.tick();
    for (const int q : {13, 12, 11, 10})
        CHECK(chip.get(q));
}

// A MODE that fell three ticks (25 ns) before the clock is seen: the edge shifts.
TEST_CASE("К555ИР16: MODE falling three ticks before the clock shifts")
{
    using V = Vk555ir16;
    Chip<V> chip({PIN(1), PIN(2), PIN(3), PIN(4), PIN(5), PIN(6), PIN(8), PIN(9), PIN(10),
                  PIN(11), PIN(12), PIN(13)},
                 {OE(13), OE(12), OE(11), OE(10)});
    chip.set(8, true);
    chip.set(1, false);
    chip.set(6, false);
    for (const int p : {2, 3, 4, 5})
        chip.set(p, false);
    chip.set(9, true);
    chip.tick();
    chip.set(9, false);
    chip.tick();   // QA..QD all 0: shifted in from SER
    chip.set(9, true);
    chip.set(6, true);
    for (const int p : {2, 3, 4, 5})
        chip.set(p, true);
    chip.tick();
    chip.tick();
    chip.set(6, false);
    chip.tick();
    chip.tick();
    chip.tick();
    chip.set(9, false);
    chip.tick();
    for (const int q : {13, 12, 11, 10})
        CHECK(!chip.get(q));
}

// The data's hold (10 ns): A..D changing with the clock's fall are not
// taken - the edge loads the word that stood through the setup.
TEST_CASE("К555ИР16: data changing with the clock is not taken")
{
    using V = Vk555ir16;
    Chip<V> chip({PIN(1), PIN(2), PIN(3), PIN(4), PIN(5), PIN(6), PIN(8), PIN(9), PIN(10),
                  PIN(11), PIN(12), PIN(13)},
                 {OE(13), OE(12), OE(11), OE(10)});
    chip.set(8, true);
    chip.set(1, false);
    chip.set(6, true);
    for (const int p : {2, 3, 4, 5})
        chip.set(p, true);
    chip.set(9, true);
    for (int t = 0; t < 4; ++t)
        chip.tick();
    chip.set(9, false);
    for (const int p : {2, 3, 4, 5})
        chip.set(p, false);
    chip.tick();
    for (const int q : {13, 12, 11, 10})
        CHECK(chip.get(q));
}

// SER the same: a shift takes the SER that stood through the setup.
TEST_CASE("К555ИР16: SER changing with the clock is not taken")
{
    using V = Vk555ir16;
    Chip<V> chip({PIN(1), PIN(2), PIN(3), PIN(4), PIN(5), PIN(6), PIN(8), PIN(9), PIN(10),
                  PIN(11), PIN(12), PIN(13)},
                 {OE(13), OE(12), OE(11), OE(10)});
    chip.set(8, true);
    chip.set(1, true);
    chip.set(6, false);
    for (const int p : {2, 3, 4, 5})
        chip.set(p, false);
    chip.set(9, true);
    for (int t = 0; t < 4; ++t)
        chip.tick();
    chip.set(9, false);
    chip.set(1, false);
    chip.tick();
    CHECK(chip.get(13));
}
