# D70's OE on GND (the owner's board)

The case is told in [findings.md](../../findings.md), section «The factory
wire patches restore Э3», the paragraphs on the thin bare wire; the change
is `netlist/rev3a.csv` (D70.15 off `~D14.3`, on GND).

| File | What |
|---|---|
| `owner-d70.jpg` | the owner's board (`photos/known-machines/MC 0515 04.jpg`), three times enlarged: К555КП11 (D70) right of the resistors R24, R25, R26; the thin bare wire runs under the resistors from the corner pin of the ИР23 on the left (D57 pin 10, GND) to D70's pin row; the white wire beside it is the factory patch D98.3 - D70.11 |
| `bare-wire-labelled.jpg` | the bare wire four times enlarged, its ends ringed: D57.10 (the top pin of the ИР23's right row, GND) and D70.15 (the КП11's left row, second from the bottom - the pin lifted off its pad); D70.11 above it is where the factory's white wire lands. The D57.10 ring sits about half a pin left of where the wire starts - the photo's resolution |
| `owner-d70-labelled.jpg` | the same corner on the labelled crop of the photo (`annotated-MC 0515 04-wires.jpg`) |
| `drawing-d70.jpg` | the assembly drawing 3.858.420 СБ (page 27, turned half round) at D70: the patch to D70.11 is drawn, a wire to D70.15 is not - a later fix |

That D70's pin 15 is lifted off its pad the owner saw on the photo
(2026-10-20); it cannot be made out at this size.
