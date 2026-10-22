# The factory wire patches

The case is told in [findings.md](../../findings.md), sections «The factory
wire patches restore Э3» and «The owner's board against another one»: the
patches drawn on the assembly drawing join pins Э3 puts on one net and the
copper does not, so Э3 is the patched board.

| File | What |
|---|---|
| `drawing-whole.jpg` | the assembly drawing 3.858.420 СБ (`mc0515-ed`, page 27), turned half round so the references read; the patches are the thin lines between pins, each end marked with its pin number |
| `drawing-notes.jpg` | its notes; note 11: «Поднять ножки микросхем D84/4, D17/3, D33/3» - the three pins lifted off their pads for the patches |
| `owner-board.jpg` | the owner's НС4 rev. 3А (`photos/known-machines/MC 0515 04.jpg`) with its white wire patches |
| `owner-board-labelled.jpg` | the same photo with every patch's ends ringed and numbered: 1 D13.1 - D84.4 (`АПК7 Н`), 2 D17.3 / D33.3 - D47.5 (`ИМП ЧТ Н`), 3 D15.9 - D40.9 (`ТС8М L`), 4 D98.3 - D70.11 (`СТРОБ Н`); 5 (green) is the owner's board's own bare wire D57.10 - D70.15, see [d70-oe-gnd](../d70-oe-gnd/) |
| `patch1-d13-d84.jpg` | patch 1 end to end, twice enlarged: from D13's pin 1 (the РТ12 with the paper label) down the left and along the row above the ROMs to D84's pin 4 (the К555ЛЛ1 at the bottom right) |
| `patch2-d17-d33-d47.jpg` | patch 2: the pair from D17.3 and D33.3 (two КР1533ТМ2 either side of the ЛР11 D25) joined under the capacitors and run down to D47.5 |
| `patch3-d15-d40.jpg` | patch 3: D15.9 (the ЛН1 - a КР1533 part on this board) down and right to D40.9 (the ТМ8) |
| `patch4-d98-d70.jpg` | patch 4: D70.11 (the К555КП11 at the top left) down past D64, along under the ROMs and the processor and down the right side to D98.3 (the КР1533ТМ2 at the board's right edge) |
| `owner-wires-labelled.jpg` | the left middle of that photo, the parts labelled: the wire D13.1 - D84.4 leaving D13 between the two РТ12s, down past D14 and D22, along the row above D17..D58; the wire D98.3 - D70.11 coming in from the right under D64/D71 to D70; the pair at D17/D33 |
The pins on the owner's photo are placed from each chip's centre, read
off the photo on a grid, and the copper's footprint round it (the photo
is turned 90 degrees to the layer pixels; the 2.5 mm pin pitch is about
12 photo pixels).  The rings land on the wires' ends to within a pin.

| File | What |
|---|---|
| `frolov-board.jpg` | another machine (Sergei Frolov's museum photo, leningrad.su/museum, 2009; `mc0515-6.jpg`, turned 90 degrees to the owner's): the same white wires on the same routes - factory work, not one owner's repairs |
