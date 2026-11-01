// КР580ВР43 (8243): the MCS-48's port expander, against the 8243
// datasheet's protocol.  As PROG falls the four lines P20..P23 give the
// order - P21 P20 the port (0 P4, 1 P5, 2 P6, 3 P7), P23 P22 what to do
// (0 read, 1 write, 2 OR, 3 AND); to write, OR or AND the processor puts
// the data on P20..P23 while PROG is low and the port takes it as PROG
// rises, its outputs on from then; to read, the expander puts the port's
// pins on P20..P23 while PROG is low, the port's outputs off.  With CS high
// nothing happens.  After power-on every port is off.
#include "chip.h"

#include "Vkr580vr43.h"

// PIN_IN(n): what a pin both driven and read reads, pN_in - set as pin -n
#define PIN_IN(n) {-n, [](V& m) -> CData& { return m.p##n##_in; }}

namespace {

using V = Vkr580vr43;

// the four pins of each port, bit 0 first
const int kPort[4][4] = {{2, 3, 4, 5}, {1, 23, 22, 21}, {20, 19, 18, 17}, {13, 14, 15, 16}};
const int kP2[4] = {11, 10, 9, 8};

Chip<V> make()
{
    return Chip<V>(
        {PIN(1), PIN(2), PIN(3), PIN(4), PIN(5), PIN(6), PIN(7), PIN(8), PIN(9), PIN(10), PIN(11),
         PIN(13), PIN(14), PIN(15), PIN(16), PIN(17), PIN(18), PIN(19), PIN(20), PIN(21), PIN(22), PIN(23),
         PIN_IN(1), PIN_IN(2), PIN_IN(3), PIN_IN(4), PIN_IN(5), PIN_IN(13), PIN_IN(14), PIN_IN(15), PIN_IN(16), PIN_IN(17), PIN_IN(18), PIN_IN(19),
         PIN_IN(20), PIN_IN(21), PIN_IN(22), PIN_IN(23), PIN_IN(8), PIN_IN(9), PIN_IN(10), PIN_IN(11)},
        {OE(1), OE(2), OE(3), OE(4), OE(5), OE(13), OE(14), OE(15), OE(16), OE(17), OE(18), OE(19),
         OE(20), OE(21), OE(22), OE(23), OE(8), OE(9), OE(10), OE(11)});
}

// the processor's side of P20..P23
void p2(Chip<V>& chip, int nibble)
{
    for (int i = 0; i < 4; ++i)
        chip.set(-kP2[i], ((nibble >> i) & 1) != 0);
}

// one MOVD/ORLD/ANLD to port `port` (0..3 for P4..P7)
void order(Chip<V>& chip, int op, int port, int data)
{
    p2(chip, op << 2 | port);
    chip.tick();
    chip.set(7, false);                 // PROG falls: the order taken
    chip.tick();
    p2(chip, data);
    chip.tick();
    chip.set(7, true);                  // PROG rises: the data taken
    chip.tick();
}

int port_out(Chip<V>& chip, int port)
{
    int v = 0;
    for (int i = 0; i < 4; ++i)
        v |= chip.get(kPort[port][i]) ? 1 << i : 0;
    return v;
}

bool port_on(Chip<V>& chip, int port)
{
    for (int i = 0; i < 4; ++i)
        if (!chip.driven(kPort[port][i])) return false;
    return true;
}

}  // namespace

TEST_CASE("КР580ВР43: ports off after power-on; write, OR and AND each port")
{
    Chip<V> chip = make();
    chip.set(6, false);                 // CS
    chip.set(7, true);                  // PROG
    chip.tick();
    for (int port = 0; port < 4; ++port)
        CHECK(!port_on(chip, port));
    for (int port = 0; port < 4; ++port) {
        order(chip, 1, port, 0x5 + port);
        CHECK(port_on(chip, port));
        CHECK(port_out(chip, port) == 0x5 + port);
        order(chip, 2, port, 0x8);
        CHECK(port_out(chip, port) == ((0x5 + port) | 0x8));
        order(chip, 3, port, 0x9);
        CHECK(port_out(chip, port) == (((0x5 + port) | 0x8) & 0x9));
    }
}

TEST_CASE("КР580ВР43: a read puts the port's pins on P20..P23 while PROG is low")
{
    Chip<V> chip = make();
    chip.set(6, false);
    chip.set(7, true);
    order(chip, 1, 2, 0xF);             // P6 written
    for (int i = 0; i < 4; ++i)
        chip.set(-kPort[2][i], i == 1 || i == 3);   // what the port's pins read: 0xA
    p2(chip, 0 << 2 | 2);
    chip.tick();
    chip.set(7, false);
    chip.tick();
    chip.tick();
    CHECK(!port_on(chip, 2));
    int v = 0;
    for (int i = 0; i < 4; ++i) {
        CHECK(chip.driven(kP2[i]));
        v |= chip.get(kP2[i]) ? 1 << i : 0;
    }
    CHECK(v == 0xA);
    chip.set(7, true);
    chip.tick();
    for (int i = 0; i < 4; ++i)
        CHECK(!chip.driven(kP2[i]));
}

TEST_CASE("КР580ВР43: with CS high nothing is taken")
{
    Chip<V> chip = make();
    chip.set(6, true);
    chip.set(7, true);
    chip.tick();
    order(chip, 1, 0, 0x6);
    CHECK(!port_on(chip, 0));
}
