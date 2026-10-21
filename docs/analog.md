# Analog parts and their digital effect

The board is logic on one master clock (`CLAUDE.md`).  Every part that is
not a logic chip is replaced by its effect on the logic, or left out, and
each decision is a row of `netlist/analog.csv` with its reason;
`tools/board.py` refuses to build a board with a part that has none.  Two
rules need no row: a part with all its pins on power is decoupling and is
left out, and a resistor with a pin on +5 V is a pull-up.

This page groups those rows and keeps the places still open - the ones the
running board will settle (plan 6b).

## The master clock

120 MHz: the 8 MHz crystal is a period of 15 ticks, the 15 MHz one of 8,
and a tick (8.3 ns) is about one TTL propagation delay.  A chip with state
shows its stored state on its outputs; its clock edges, asynchronous
inputs and transparent gates act at the next tick (`rtl/chips/README.md`).
That is what breaks the board's loops through flip-flops: D47 resetting
itself through D93 (the `ИМП ЧТ` pulse), the RAM-refresh handshake through
D43/D98/D24, and the bus loop through the address latch D94.

## Levels

- **Pull-ups** to +5 V hold a net at 1 while nothing drives it; a net with
  no driver and no pull is read as 1, as a floating TTL input is.
- **Strong pull-downs** (20 Ohm on the OE of D51, D57, D78, D89, D92, D94;
  1 kOhm on `УСТ L`) hold their net at 0.
- **Weak resistors to ground on a TTL input** (10 kOhm on the E of D20 and
  D28) are left out: the input still reads high.
- **Series resistors** (33 Ohm on RAS/CAS, 20 Ohm into D86's OE, 43 Ohm to
  the speaker, 620 Ohm into D47's reset) are wires.

## Line levels

The С2 port and the keyboard line are RS-232 levels; in logic a 1 stands
for a positive line.  The receiver D48 (КР559ИП11) gives 1 while its A
input is above B, and also for two equal levels (its fail-safe for an
open line); its B inputs sit on the lines, its A inputs on ground, so it
inverts.  The 23.7 kOhm resistors to -12 V on the lines are pulls to 0:
an idle line is negative, the mark.  The 2200 pF filters are left out.
The drivers D20, D28 (КР1102АП15) invert; their VEE diodes are left out.

## Oscillators

The crystals BQ1 (8 MHz, around D15's gates 6 and 5) and BQ2 (15 MHz,
around D91's gates 1 and 2) are each an instance of `rtl/chips/crystal.sv`:
the oscillator's square wave, made of the 120 MHz master clock, on the
crystal's pin 1, which the coupling capacitor (a wire) joins to the first
gate's input (`~D15.13`, `~D91.1`); the gates take it on as on the board.
Pin 2, on the second gate's output, is left; the bias resistors are left
out.  The period and its high part in master ticks come from the rating in
parts.csv: 8 MHz is 15 ticks, 8 of them high; 15 MHz is 8 ticks, 4 high.

They were first brought out, the test bench driving the wave; inside, the
board runs on the master clock alone, as it will in the FPGA.

## МС 7007 keyboard

The matrix diodes VD5..VD15 are wires (a pressed key pulls a line low
through its diode).  The detector - `ДКЛ08..18` summed into `ВБКЛ L`
through the 10 kOhm pack DA2, compared by D48's third receiver with the
mid-supply divider R18/R19 - is brought out (DA2) for now; the divider is
the low side of that receiver's pair, so `~D48.11` follows the inverse of
`ВБКЛ L`.

## Cassette

The input (C72, R64, R66, R69, the comparator D90 with R65, R70) reaches
XS2 only through SA1's even contacts, open on the owner's build: its
inputs rest low, its output `ПРМАГ Н` low.  The output divider and
coupling (R71, R72, C83, C84) are left out: `ПРДМАГ Н` is the signal.

## Video

The DAC (VT1, VT2 and their resistors, trimmers and bias) is left out: the
frame is taken from its digital inputs, the outputs of D75 and D81 and
`ПСС L`, which are board outputs.

## LEDs and the speaker

The LEDs VD9, VD16, VD17 are left out (their cathode nets are board
outputs); the speaker clamp VD13 too.

## Open

- **RC edges on the sync PROM outputs** - C35 (68 pF on `СГИ L`), C64
  (120 pF on `ССИ L`), C70 (100 pF on `БОРДС L`): open-collector outputs
  with pull-ups, so the rising edges are slow, tens of ns.  Left out for
  now; a delay of a few ticks if the frame needs it.
- **C40** (27 pF on D47.9 into D14.12): a few ns.  Left out.
- **C82 with R78** (about 30 ns) on D47's reset sets the width of the
  `ИМП ЧТ` pulse with the gate delays; left out, the pulse is a tick or
  two wide.  The first candidate for a delay of its own.
- **C86** (0.1 uF on D95.7, an output of the inverting buffer D95, into
  D16.12, the reset of D16's second counter: `СТАРТ Н`, `СТ1 Н`): slows
  that output by tens of microseconds, a power-on shape.  Left out: the
  counter starts from the simulation's initial state.  To model with the
  power-on sequence.
- **VD11 with R58**: about 1.7 V on D95.13, which an LS input reads high;
  left out, so the input reads high.  What it is for is still to find.
- **The МС 7007 detector** (DA2 above).
