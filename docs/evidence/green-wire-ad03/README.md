# The green wire from АД03 (the owner's board) - open

The case is told in [findings.md](../../findings.md), section «The green
wire on the owner's board»; nothing is in the netlist for it yet.

| File | What |
|---|---|
| `owner-green-wire.jpg` | the owner's board (`photos/known-machines/MC 0515 04.jpg`), twice enlarged: the green wire from the КР1533ИР22 right of the processor (D92, pin 9 = Q3 = `АД03 Н`) down past D86 and to the left, to the upper lead of the resistor beside the КР1533ИД7 (D80) at the bottom |
| `drawing-d80-d85.jpg` | the assembly drawing (3.858.420 СБ, page 27, turned half round) there: the resistor between D80 and D85 is **R39** - 1 kOhm pulling `БОРДС L` up to +5V; no patch is drawn to it |

Which of R39's two leads the wire takes, and whether a trace beside it is
cut, the photo does not show.  The owner reads the end as D80's fourth pin
from the top (2026-11-03): the solder lies over R39 without touching it -
D80 pin 5 (G2B), by the chip's turn (its label reads as text turned a
quarter clockwise); on Э3 it is `ВБРЕГ L` with G2A (`docs/findings.md`).

| File | What |
|---|---|
| `closeup-green-wire.jpg` | `photos/known-machines/16337efabef4.jpg` (2560 px, 2015, turned half round), twice enlarged: the green wire's upper end, on the lower of two КР1533ИР22 (date 9010) - it bends down onto the left end of that chip's upper row, pin 9 or pin 10 by where it lands (pin 10 is GND, pin 9 Q3 = `АД03 Н`) - the owner reads the second pin from that end, pin 9 (2026-11-03), as on the other photo; it runs right under the upper ИР22's pins and over one of the board's resistors, nothing in series |

