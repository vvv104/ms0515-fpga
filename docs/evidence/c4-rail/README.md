# C4: -12 V on Э3, +12 V on the copper

The case is told in [findings.md](../../findings.md), section «The 1991
placement off the assembly drawing», «Against Э3: C4».  Taken from the
copper (the owner, 2026-11-10).

| File | What |
|---|---|
| `e3-sheet1-filters.png` | 3.858.420 Э3 sheet 1 (`mc0515-ed`, page 18): the supply filters - C1, C23, C31 on +12 V (XP2:33..38), C10, C4, C9 on -12 V (XP2:39,40) |
| `mick-all-filters.png` | Mick's four layers together (`radon17-layers/NS4-all.gif`) round C4 by D1 and XP2, round C9, C10, C1 at the supply corner, round C23, C31; the pads named by `netlist/placement-1991.csv` |
| `mick-layers-c4.png` | the same place round C4, each layer alone: on layer 1 a wide run from C4.1 to D1.40 and on to XP2.38 (+12 V), apart from the +12 V run to XP2.34; the -12 V run (layer 8) on XP2.40, away from C4; C4.2 on no layer (a plane) |
| `owner-board-c4.jpg` | the owner's board (`photos/known-machines/MC 0515 04.jpg`): a К10-17 where C4 stands, at D1's pin-1 end; the four К53-14 named as the placement has them; C9 not to be seen |

