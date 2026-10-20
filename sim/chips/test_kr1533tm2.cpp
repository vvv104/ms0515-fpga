// КР1533ТМ2 (74ALS74): the К555ТМ2 function; a clock edge and the
// asynchronous inputs reach its pins.
#include "chip.h"

#include "Vkr1533tm2.h"

TEST_CASE("КР1533ТМ2: wraps К555ТМ2")
{
    using V = Vkr1533tm2;
    Chip<V> chip({PIN(1), PIN(2), PIN(3), PIN(4), PIN(5), PIN(6), PIN(8), PIN(9), PIN(10),
                  PIN(11), PIN(12), PIN(13)});
    for (int pin : {1, 4, 10, 13})
        chip.set(pin, true);
    chip.set(2, true);
    chip.set(12, false);
    chip.tick();
    chip.set(3, true);
    chip.set(11, true);
    chip.tick();
    CHECK(chip.get(5));
    CHECK(!chip.get(9));
    chip.set(1, false);
    chip.set(10, false);
    chip.eval();
    CHECK(!chip.get(5));
    CHECK(chip.get(9));
}
