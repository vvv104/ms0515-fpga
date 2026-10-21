// power: the bench as the machine's supply.  While the supply rises, the
// monitor VD11/R58 puts a low on D95.13 (brought out as x_VD11_p1), which
// resets D16's second counter so that СТАРТ starts from a cleared counter
// (docs/analog.md).  The supply's rise time is not known; a millisecond.
#pragma once

#include <cstdint>

constexpr std::uint64_t kSupplyRiseTicks = 1000ull * 120ull;     // 1 ms of the 120 MHz master clock

// nothing on the connectors: every input of the board off (a model's
// inputs power up as random as its state; rtl/board/ns4_inputs.inc lists
// them)
template <class V>
void disconnect(V& m)
{
#define NS4_INPUT(name) m.name = 0;
#include "../../rtl/board/ns4_inputs.inc"
#undef NS4_INPUT
}

// set the monitor's node for master tick t from power-on, before the tick
template <class V>
void power(V& m, std::uint64_t t)
{
    m.x_VD11_p1 = 0;
    m.x_VD11_p1_oe = t < kSupplyRiseTicks ? 1 : 0;
}
