# R45: D86's OE on GND - once misread as on РПРРТ H

The case is told in [findings.md](../../findings.md), section «R45: D86's
OE - on GND, misread as on РПРРТ H»: the netlist had R45 on D97's
`РПРРТ H`; the sheet has it on ground, the ground sign drawn over that
wire's label.  Here are the pictures.

## The schematic

| File | What |
|---|---|
| `schematic-sheet2.jpg` | 3.858.420 Э3 sheet 2 (`mc0515-ed`, page 19), the place boxed; R62 at D92's EO (upper left) is drawn to ground as well |
| `schematic-r45.jpg` | the place: D78's EO (pin 1) through R43 to a ground sign (green); D86's EO (pin 7) through R45 (red) - its wire going down towards the label «2 РПРРТ Н (Л2)» |
| `schematic-r45-ground.jpg` | the same at the scan's full resolution: R45's wire ends in a ground sign whose bar lies on the top of the letters «РП» (ringed); D97.2's wire `РПРРТ Н` runs under the label |

## The board model

| File | What |
|---|---|
| `screen-as-drawn.png` | the board as the netlist misread it (R45 on `РПРРТ H`), 11.26 s after power-on: Omega's first line («ОМЕГА SJ(S) V05.04») at the foot of the screen, the cursor still under «НГМД готов» |
| `screen-r45-gnd.png` | R45 on GND, as Э3 has it, 12.5 s after power-on: the line right under «НГМД готов», as on the emulator |

## The bare board, rev. 3А

Crops of radon.su's scans `NS4_red3A_1.tif` / `NS4_red3A_8.tif`, warped by
`netlist/registration.json` and turned to the component side's view; the
red ring is the pad named.  Side 1 shows its own tin silver and layer 2
gold through the laminate; side 8 likewise with layer 7.

| File | What |
|---|---|
| `scan-d86-7-side1.jpg`, `-side8.jpg` | D86 pin 7 (OE): no trace reaches it on either side |
| `scan-pad100-side1.jpg`, `-side8.jpg` | the via on `РПРРТ H` between D97.2 and D60.2 - the net's only other pad on the outer layers; no resistor pad on it |
| `scan-r43-side1.jpg`, `-side8.jpg` | for comparison: the R43 end that joins D78.1 (OE, held to GND) |
| `scan-r24-side1.jpg`, `-side8.jpg` | the same for R24 and D51.1 |

## The assembled boards

| File | What |
|---|---|
| `owner-board.jpg` | the owner's НС4 rev. 3А (`photos/known-machines/MC 0515 04.jpg`), the area below boxed |
| `owner-d86.jpg` | that area: КР1533ИР23 (D78) left, КР1533КП15 (D86) right of the boxed resistors - left to right R41 (1 kOhm, `СБРКС L` to +5V), R43 (20 Ohm, D78.1 to GND), R45 (20 Ohm, banded like R43; no outer-layer copper at either end) |
| `drawing-r41-r43-r45.jpg` | the assembly drawing 3.858.420 СБ (page 27, turned half round): R41, R43, R45 stacked between D78 and D86, R45 next to D86 - the order the copper gives |
| `frolov-corner.jpg` | another machine (Sergei Frolov's museum photo, leningrad.su/museum, 2009; `mc0515-7.jpg`): the same corner turned 180 degrees - ИР23 above, the three resistors, «…15» below |

The resistors were placed by mapping the photo onto the copper through
the two ROMs and the processor: the three resistor bodies fall on the pad
pairs at y = 1023, 1062, 1102 of Mick's layer pixels, x 3228 to 3425.
