# The routed board's power: a first check

The board of `docs/plan.md` 8d as routed by `tools/pathfind.py` (2026-11-10):
the 1991 placement and outer copper, the planes In2 (`GND`) and In5 (`+5V`),
the inside routed.  What it was checked for: whether the planes and the +12 V
copper carry the board's current without a drop the chips would notice.
Nothing here was measured on a board; the inputs that are estimates are marked so.

## Inputs

| What | Value | From |
|---|---|---|
| The board's current, +5 V | at most 2 A | НС4 technical description, table 2 item 11 |
| +12 V, -12 V | at most 0.1 A, 0.02 A | the same |
| Supply tolerance | +5 V ± 0.25 V, +12 V ± 0.6 V | the same, section 2 |
| The supply's entry | XP2: +5 V pins 1..12, GND 13..32, +12 V 33..38 | Э3 |
| Each chip's share of the 2 A | its series' typical Icc (the analogue's data sheet), all scaled to 2 A | ESTIMATE |
| Copper | 35 um, 0.49 mOhm a square | ESTIMATE (the usual foil) |
| The planes | KiCad's fill of In2 and In5, the antipads of every hole cut out | the routed board |
| In2 to In5 | 0.7 mm, glass epoxy (er 4.7) | ESTIMATE |
| Decoupling | 61 x К10-17 0.22 uF Н90 (taken as 0.15 uF at 5 V), 1 x 2200 pF, 2 x К53-14 33 uF on +5 V; 3 x 0.22 uF and 1 x 33 uF on +12 V; where ПЭ3's parts stand on the board | ПЭ3, the board; the Н90 loss with bias an ESTIMATE |
| A К10-17's leads, a К53-14's | 4 nH, 0.05 Ohm; 10 nH, 0.8 Ohm | ESTIMATE |
| A DIP's supply leads, the chip's own capacitance | 12 nH; 200 pF with 1 Ohm | ESTIMATE |
| The cable to the supply | 200 nH, 20 mOhm | ESTIMATE |
| A КР565РУ5Г's current | its average plus 80 mA / 30 ns at RAS fall, 40 mA / 20 ns at CAS fall, 80 mA / 30 ns at RAS rise; the 16 together every 400 ns (the base microcycle) | ESTIMATE from 4164 data sheets |
| ВГ93's +12 V current | 10 mA and a 30 mA / 20 ns pulse every 1 us | ESTIMATE (FD1793) |

## Static: the planes' drop (2 A)

The planes as a 0.2 mm grid of resistors, the chips' currents drawn at their
pins, XP2 held.  The worst chip (the processor in XS6 and its neighbours
D84..D99, farthest from XP2): **1.9 mV** below on +5 V, **1.8 mV** above on
GND - 3.7 mV of the 250 mV allowed.  Where a row's antipads run together the
plane is slotted, and the current goes round; it costs nothing to speak of.

## Dynamic: the DRAM bank on +5 V

The plane pair as 5 mm cells (each cell's capacitance, between neighbours the
loop's inductance and the copper's resistance, by the copper left in them), the
capacitors where they stand, the supply through its cable, the 16 DRAMs
pulsing together, every other chip's average current.  Over four cycles:

| | Board as made | Without the 8 x 0.22 uF next to the DRAMs |
|---|---|---|
| The planes at a DRAM, mean below 5 V | 54 mV (40 of them the cable) | 55 mV |
| The planes at a DRAM, worst | 81..95 mV | 90..114 mV |
| Behind a DRAM's leads, worst | 196..212 mV | 204..230 mV |
| The planes at the processor, worst | 64 mV | 68 mV |

The planes themselves move 30..40 mV on the bank's edges; most of what a DRAM
sees is its own leads (identical on any board with these packages).  Within
the 250 mV, with the margins of estimates such as these.

## Dynamic: +12 V to КР1818ВГ93 (pin 40) and C4

The +12 V copper as routed: XP2 to XS5.40 43 mm of 0.25 mm run (mostly F.Cu),
C4 5 mm beyond the pin, C1 (33 uF) on its own branch 60 mm from XP2, C23/C31
and the КР1102АП15s about 145 mm away on a third.

| | Below 12 V at pin 40, worst | Ripple at pin 40 |
|---|---|---|
| C4 on +12 V (Mick's copper; this board) | 37 mV | 75 mV p-p |
| C4 off +12 V (Э3 draws it on -12 V) | 447 mV | 949 mV p-p |

Without C4, ВГ93's +12 V is decoupled only by C1 some 100 mm of copper away,
and the run's and the cable's inductance ring with the chip: the dip reaches
most of the 0.6 V allowed.  The pulse is an estimate, the twelvefold
difference is not: C4 is ВГ93's own capacitor, as the copper has it
(`docs/findings.md`, C4).
