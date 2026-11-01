# The interrupt encoder: D89 to D96 on the copper and on the owner's board

The case is told in [findings.md](../../findings.md), section «Interrupt
priority (Э3 sheet 2 vs. OSA)»: does the rev. 3А board wire the request
latch D89 (ИР23) to the priority encoder D96 (ИВ3) as Э3 draws it?

| D89 (Q) | D96 (input) | request | on the copper |
|---|---|---|---|
| 2 (Q0) | 12 (I2) | `ЗАПР МОН` | layer 2 down from D96.12 to a via, layer 7 to the right and up into D89.2 |
| 5 (Q1) | 13 (I3) | `ЗАПР 7007` | layer 8 straight down from D96.13, a step to the left into D89.5; the run beside it from D89.4 down is `ЗАПР 7007 L` itself (to D66.6) and does not touch it |
| 6 (Q2) | 1 (I4) | `ЗАПР Д С2` | layer 7 from D96.1 to a via, layer 2 down into D89.6 |
| 9 (Q3) | 2 (I5) | `ЗАПР М С2` | layer 7 from D96.2 to a via, layer 2 down into D89.9 |
| 12 (Q4) | 4 (I7) | `ЗАПР ТАЙМ` | layer 2 from D96.4 to a via under D89's lower row, layer 7 to the left, layer 2 into D89.12 |
| 15 (Q5) | 10 (I9) | `ЗАПР КЛ` | layer 8 from D96.10 down the gap between the rows into D89.15 |

D96.3 and D96.5 (I6, I8) are joined on layer 1 (`Р4 H`).  Every one as
Э3 has it, and as Mick's traces have it (`tools/compare.py` finds the
traces at one with Э3 on every pin of D89 and D96).

## The bare board, rev. 3А

Crops of radon.su's scans `NS4_red3A_1.tif` / `NS4_red3A_8.tif`, warped
by `netlist/registration.json` onto the layer pixels (component side's
view).  Side 1 shows its own tin (layer 1, silver) and layer 2 (gold)
through the laminate; side 8 likewise layers 8 and 7.  The pins are
ringed and numbered (`96.4` = D96.4).

| File | What |
|---|---|
| `scan-side1.jpg`, `scan-side8.jpg` | D96 (top) and D89 (bottom), both sides |
| `zoom-side1.jpg`, `zoom-side8.jpg` | the same, four times the layer pixels: the vias of `ЗАПР ТАЙМ`, `М С2`, `Д С2`, `МОН` |
| `zoom-d96-13-side8.jpg` | D96.13 down to D89.5 beside D89.4 - the two runs apart |

## The owner's board

| File | What |
|---|---|
| `owner-d89-d96.jpg` | `photos/known-machines/MC 0515 04.jpg` twice enlarged: D89 (КР1533ИР23) and D96 (К555ИВ3) at the lower right, the processor above.  No wire on either chip and no pin lifted; the green wire from D92.9 (`АД03`, see [green-wire-ad03](../green-wire-ad03/)) and the white patch to D98.3 pass above them.  At about 4 photo pixels a pin, a cut on the outer copper would not show. |
