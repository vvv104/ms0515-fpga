// К589ИР12 (8212): an 8-bit latch with three-state outputs and a service
// request, against the 8212 datasheet's function: DS = !DS1 & DS2; with MD
// high the outputs are on and DS clocks the latch, with MD low DS turns the
// outputs on and STB clocks it; the latch lets its inputs through while
// clocked and holds after; CLR low clears it; INT falls as STB falls and
// rises as the device is selected.
#include "chip.h"

#include "Vk589ir12.h"

namespace {

using V = Vk589ir12;

const int kDi[8] = {3, 5, 7, 9, 16, 18, 20, 22};
const int kDo[8] = {4, 6, 8, 10, 15, 17, 19, 21};

Chip<V> make()
{
    return Chip<V>({PIN(1), PIN(2), PIN(3), PIN(4), PIN(5), PIN(6), PIN(7), PIN(8), PIN(9), PIN(10),
                    PIN(11), PIN(13), PIN(14), PIN(15), PIN(16), PIN(17), PIN(18), PIN(19), PIN(20),
                    PIN(21), PIN(22), PIN(23)},
                   {OE(4), OE(6), OE(8), OE(10), OE(15), OE(17), OE(19), OE(21)});
}

void put(Chip<V>& chip, int value)
{
    for (int i = 0; i < 8; ++i)
        chip.set(kDi[i], ((value >> i) & 1) != 0);
}

int out(Chip<V>& chip)
{
    int v = 0;
    for (int i = 0; i < 8; ++i)
        v |= chip.get(kDo[i]) ? 1 << i : 0;
    return v;
}

bool all_driven(Chip<V>& chip, bool on)
{
    for (int pin : kDo)
        if (chip.driven(pin) != on) return false;
    return true;
}

}  // namespace

TEST_CASE("К589ИР12: output mode (MD high) - DS clocks the latch, the outputs always on")
{
    Chip<V> chip = make();
    chip.set(2, true);                  // MD
    chip.set(11, false);                // STB
    chip.set(14, true);                 // CLR off
    chip.set(1, false);                 // DS1 low
    chip.set(13, true);                 // DS2 high: selected, the latch open
    put(chip, 0x5A);
    chip.tick();
    CHECK(out(chip) == 0x5A);
    CHECK(all_driven(chip, true));
    chip.set(13, false);                // deselected: held
    chip.tick();
    put(chip, 0xC3);
    chip.tick();
    CHECK(out(chip) == 0x5A);
    CHECK(all_driven(chip, true));
    chip.set(13, true);
    chip.tick();
    CHECK(out(chip) == 0xC3);
}

TEST_CASE("К589ИР12: input mode (MD low) - STB clocks, DS turns the outputs on; CLR clears")
{
    Chip<V> chip = make();
    chip.set(2, false);
    chip.set(14, true);
    chip.set(1, true);                  // not selected
    chip.set(13, false);
    chip.set(11, true);                 // STB high: the latch open
    put(chip, 0x81);
    chip.tick();
    CHECK(all_driven(chip, false));
    chip.set(11, false);                // the strobe falls: held, a request
    chip.tick();
    put(chip, 0x18);
    chip.tick();
    CHECK(!chip.get(23));
    chip.set(1, false);
    chip.set(13, true);                 // selected: out, the request withdrawn
    chip.tick();
    CHECK(all_driven(chip, true));
    CHECK(out(chip) == 0x81);
    CHECK(chip.get(23));
    chip.set(14, false);                // cleared
    chip.tick();
    CHECK(out(chip) == 0x00);
}
