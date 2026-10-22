# The green wire from АД03 (the owner's board) - open

The case is told in [findings.md](../../findings.md), section «The green
wire on the owner's board»; nothing is in the netlist for it yet.

| File | What |
|---|---|
| `owner-green-wire.jpg` | the owner's board (`photos/known-machines/MC 0515 04.jpg`), twice enlarged: the green wire from the КР1533ИР22 right of the processor (D92, pin 9 = Q3 = `АД03 Н`) down past D86 and to the left, to the upper lead of the resistor beside the КР1533ИД7 (D80) at the bottom |
| `drawing-d80-d85.jpg` | the assembly drawing (3.858.420 СБ, page 27, turned half round) there: the resistor between D80 and D85 is **R39** - 1 kOhm pulling `БОРДС L` up to +5V; no patch is drawn to it |

Which of R39's two leads the wire takes, and whether a trace beside it is
cut, the photo does not show.
