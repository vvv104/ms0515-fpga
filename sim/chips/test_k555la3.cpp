// К555ЛА3 (74LS00): four 2-input NAND gates, checked against its function
// table over every input combination.
#include <doctest/doctest.h>

#include "Vk555la3.h"

namespace {

struct Gate {
    int a, b, y;   // pin numbers
};

constexpr Gate kGates[] = {{1, 2, 3}, {4, 5, 6}, {9, 10, 8}, {12, 13, 11}};

void set_pin(Vk555la3 &chip, int pin, bool level)
{
    switch (pin) {
    case 1: chip.p1 = level; break;
    case 2: chip.p2 = level; break;
    case 4: chip.p4 = level; break;
    case 5: chip.p5 = level; break;
    case 9: chip.p9 = level; break;
    case 10: chip.p10 = level; break;
    case 12: chip.p12 = level; break;
    case 13: chip.p13 = level; break;
    default: FAIL("not an input pin: " << pin);
    }
}

bool get_pin(const Vk555la3 &chip, int pin)
{
    switch (pin) {
    case 3: return chip.p3;
    case 6: return chip.p6;
    case 8: return chip.p8;
    case 11: return chip.p11;
    default: FAIL("not an output pin: " << pin); return false;
    }
}

}  // namespace

TEST_CASE("К555ЛА3: every gate is a NAND of its own two inputs")
{
    Vk555la3 chip;
    for (int pattern = 0; pattern < 256; ++pattern) {
        for (int g = 0; g < 4; ++g) {
            set_pin(chip, kGates[g].a, (pattern >> (2 * g)) & 1);
            set_pin(chip, kGates[g].b, (pattern >> (2 * g + 1)) & 1);
        }
        chip.eval();
        for (int g = 0; g < 4; ++g) {
            const bool a = (pattern >> (2 * g)) & 1;
            const bool b = (pattern >> (2 * g + 1)) & 1;
            CHECK(get_pin(chip, kGates[g].y) == !(a && b));
        }
    }
}
