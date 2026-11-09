# The left edge of a 640 x 200 line: D70, БОРДС L and C70

**Still open** (2026-11-09): the chips as fitted and the К555ИР16's MODE
setup by corner won the edge without C70; with C70 modelled it is 12 ns
short again.  The frame's first word is the board's own, word 8000.
`docs/findings.md`, D70, "C70 modelled".  What follows is the question
as it was put.

Gathered for the owners of working MS 0515 machines (zx-pk.ru), so that
everything the question rests on can be looked at in one place.  The
whole case is told in [findings.md](../../findings.md), section «The
factory wire patches restore Э3», «What it does, on the model».

## 1. Where we started

The board НС4 is rebuilt gate by gate: a model of 3.858.420 Э3, each
chip with its datasheet delays, checked against the emulator point for
point and fetch for fetch.

The first model was Э3 with the changes seen on the owner's board
(made 07.1991): the factory's wire patches and, among them, D70's pin 15
(К555КП11) bent up off its pad and tied by a bare wire to ground
(picture 3).  That board worked: 320 x 200 and 640 x 200 as the
emulator's, Omega boots and draws right.

The other boards seen - Radon17's (the board the sync PROMs СТРВ / КДРВ
were read from) and Frolov's (leningrad.su) - have no such change at
D70.  So the owner's changes were taken for one board's, and the model
was built as Э3 draws it.

## 2. What the board as drawn gives

How it works (pictures 6 and 7):

- The points and attributes are shifted out by the К555ИР16 registers
  D49, D50, D55, D56.  Their clocks and their load/shift control (MODE)
  come through D70 as `ВК2`, `ВК3`, `ВК4`.
- D70 is off in the border: its enable `EO` (pin 15) is D14.1 = NAND
  (`БОРДС L`, `БОРДК L`).  Its outputs float, the clocks stop, the
  registers hold the word they loaded at the right edge of the line
  before.
- `БОРДС L` is an output of the PROM D21 (КР556РТ12, open collector),
  pulled up by R39 (1 kOhm), C70 (100 pF) to ground: the border's end -
  `БОРДС L` rising - comes slowly.

What the model shows with the board as drawn:

- 320 x 200: right (after the К555ИР16 fix below).
- 640 x 200: the first 16 points of every line one point to the right,
  the 16th lost.  On Omega's text it shows: «ОМ» at the left edge moved,
  the О's left stroke a point wider, the cursor a point longer (picture
  5).
- The first word of the frame's top line would come from the frame
  before if the line's first clock shifted: through the top border's
  last line `БОРДК L` keeps D70 off all the line, so nothing is loaded
  at its right edge.  As the first clock loads (above), it takes the
  word fresh: the stale word and the point's shift are two sides of
  one edge, never seen together.

The timing checked (the model's traces, 8.33 ns a tick; picture 8 draws
the same in ns from the data sheets):

- In 640 x 200 a point is ~67 ns.  The load pulse (the AND of ТС1..ТС4,
  on `ВК4`) ends with the step of the counter D30 on a ТС15 edge; the
  registers load on that same fall of `ВК2`.
- At the left edge D70 comes on by `БОРДС L` rising: D30 -> D21 -> the
  RC (R39 / C70) -> D14.1 -> D70's enable.
- For the first word to come out in place, D70 has to be on and MODE
  low before `ВК2`'s next fall, with the К555ИР16's 20 ns of MODE setup:
  no later than ~45-65 ns after D30's step.  That fall then shifts.
- With the typical delays the path takes ~100-110 ns: D70 comes on
  after that fall, and the first clock loads one shift late - the
  point's shift.
- At the right edge D70 going off (`БОРДС L` falls fast, no RC) is
  nearly level with a clock fall too (~82 ns against ~85).
- `БОРДС L` also gates the points: `ДПСЛ` = D85.2 = AND(~D49.10,
  `БОРДС L`, `БОРДК L`) goes through D39 to the video output with no
  register between.  `БОРДС L` rises with D30's step, so the gate opens
  while point 0 is already out.  With D70 always on (the owner's board),
  ~25 ns of point 0's 67 are seen without C70, none with C70.  On the
  board as drawn the gate opens on the held word's point 0, D70's first
  clock reloads it, and point 0 shows ~1.4 points wide before the rest
  of the word comes one point late.  No board as drawn shows point 0
  whole.  The model's pictures missed this: they take a point once, late
  in it.

## 3. What was tried

- The К555ИР16's model: it took MODE after the edge, and 320 x 200 had
  a stripe at the left edge.  The 74LS295's sheet takes MODE before the
  fall (setup 20 ns, hold 0) - fixed, 320 x 200 right.  640 x 200 kept
  the shift.
- Not the cause: the processor's ROM, the delay corners (all min, all
  max), the КР556РТ12's pinout (checked against the MMI 63S440), the
  address order of the СТРВ / КДРВ dumps, the PROM's other page.
- The КР556РТ12's speed: without C70, 24..36 ns draw 640 x 200 right,
  40 ns (the 82S136's typical) the shift.  With C70 (~3 ticks of rise)
  the shift even at 12 ns.
- C70 on the boards: bitten off on the owner's (the leads' stubs in the
  pads, picture 1), fitted on Frolov's (picture 2), Radon17's has that
  part of the board stripped - cannot tell.  The owner's board looks
  repaired for exactly this: C70 bitten off and D70 always on, both
  speeding this very edge up.
- The chips' spread, delays set chip by chip: only the race's own
  links can be made faster (D21, D14.1, D70's enable) - D30 and D31
  also time the video memory's reads, and the picture breaks elsewhere.
  With C70 the race is won only if `БОРДС L` reaches the threshold in
  ~20 ns or less; from R39, C70, the wiring, the four LS inputs'
  currents and the threshold that rise takes 18..46 ns, ~29 typical.  A
  board with every link at the fast end of its spread.
- The owner's two changes read as one repair of the left edge: D70
  always on removes the shift but, with C70, leaves point 0 dark; C70
  bitten off brings part of it back.  A one-point fault in the first 16
  columns of 640 x 200 is small enough to have gone unseen on serial
  boards - so the premise below may not hold, and the photos asked for
  decide it.
- The undriven lines.  While D70 is off nothing drives `ВК2`..`ВК4`;
  `ВК2` has R26 to +5 V, `ВК3` (D55/D56's clock) and `ВК4` (MODE of all
  four registers) only LS inputs.  The model reads an undriven net as 1.
  In 320 x 200 the design relies on MODE reading high: D70
  coming on makes the clock fall, and with MODE high that fall is the
  missed load (a ~130 ns window).  Read otherwise, with typical delays
  (differing samples against the emulator; 7 and 11 = the frame's first
  word only, i.e. right):

  | `ВК3`, `ВК4` undriven read as | C70 | 640 x 200 | 320 x 200 |
  |---|---|---|---|
  | 1, 1 (the model) | no | 1305 | 11 |
  | 1, 0 | no | 7 | 2453 |
  | 1, 0 | yes | 412 | 2435 |
  | 1, last driven level | no | 1305 | 11 |
  | 1, last driven level | yes | 412 | 2435 |
  | last, last | no | 7 | 707 |

  No reading draws both modes right; only MODE read low (or held) turns
  640 x 200 right.  Whether a real board can read it so was computed (a
  one-off integration, not kept): an LS input is a Schottky diode under a
  resistor from +5 V; left alone it sources current into the net until
  the gate sees 1.  Four inputs on 30..80 pF, D70's off-state leakage
  pulling down: released low, the net reads 1 after 15..80 ns and
  settles at ~1.15 V; released high, it reads 1 throughout.  Reading 0
  takes ~1 mA out of the net - a 1..2 kOhm path to ground nothing on it
  gives.  The model's reading is the board's; the readings that fixed
  the edge are not physical.  The undriven lines are ruled out.
- The chips as fitted (`netlist/fitted*.csv`, 2026-11-08): every board
  photographed has КР1533 at D30 and D14 where ПЭ3 says К555, the owner's
  included - 7.5 ns off the path's ~75 (D30 11.5 + D21 40 + D14 6 + D70's
  enable 17) against a window of ~46..66.  Not enough by the sheets; on
  the model it brought D70 on 8 ns before the clock's fall, which the
  К555ИР16's 20 ns of MODE setup does not accept (the model's register
  took MODE 8 ns before the edge and shifted; corrected to the sheet).
- The schematic read again: `ТС45 L` is `ТС15 L` (the hand's 1 looks
  like a 4); E5 is a series link, not a phase choice; the PROM's half-word
  steps exist (used at the line's end), but raising `БОРДС L` earlier
  would open `ДПСЛ` early too - no PROM content fixes the race.
- The chips.  ПЭ3 says К555 and lets К555 be fitted as КР1533; the model
  took К555.  On all three boards' photos the race's chain is КР1533:
  the video counters КР1533ИЕ10, D14 КР1533ЛА3 (pictures 9, 10; D70
  К555КП11 and D71 К555ЛИ6 as listed).  With estimated ALS typicals (TI
  prints maximums only: 8 ns for the counters, 5 for the gate) Э3's
  board draws both modes right without C70 - by less than 8 ns - and
  640 x 200 still shifted with C70, unless the КР556РТ12 answered in ~20
  ns (its handbook figure is 60 ns, a maximum).  The owner's board, on
  the same КР1533, has C70 bitten off; Frolov's has C70 fitted.

## 4. Where it stands, what is open

If hundreds of serial boards worked and only the odd one did not (as
the owner's), the typical board won this race with room to spare.  By
the data sheets it loses by ~40 ns.  The delays are not to be bent to
hide it.  The undriven `ВК3` / `ВК4` were looked at as another place
and ruled out by the inputs' physics (section 3).  With the КР1533
chips the boards carry, the race is won without C70 and lost with it:
the open question is now C70 - on Frolov's board it is fitted, and if
that machine's 640 x 200 left edge is right, the model is still ~20 ns
slow there.

Open, and asked of the owners of working machines:

1. A close photo of a working machine's screen in 640 x 200 at the left
   edge, with points at the very edge (a small program drawing vertical
   lines in columns 0-3 and 15-17 can be provided); and whether the
   left 8-16 points of the picture's top line show anything stray.
2. A continuity check on a board: where C70 (К10-17, 100 pF, by D85 on
   the D84 side) goes - to D21 pin 14 (`БОРДС L`), to D13 pin 14
   (`БОРДК L`), elsewhere; whether it is fitted; any changes at D70 pin
   15 and at R39.
3. The КР556РТ12's own figures (address access time, typical and
   maximum) from a handbook.
4. An error in the reasoning above - where else tens of ns may hide.
5. Whether R26 or any other resistor sits on `ВК3` (D70 pin 7) or
   `ВК4` (pin 9) on a real board - a pull to ground would change the
   reading computed in section 3.

## Pictures

| File | What |
|---|---|
| `1-owner-c70-bitten-off.jpg` | the owner's board at D80..D87 (`photos/known-machines/MC 0515 04.jpg`), twice enlarged, the chips named by their places and types: C70's pads empty, the leads bitten off; C69 and C73 beside it fitted; R39 under the green wire's end (the green wire is another change, `docs/evidence/green-wire-ad03`) |
| `2-frolov-c70-fitted.jpg` | Frolov's board (leningrad.su, 2009), four times enlarged at D80..D87: C70 fitted - yellow, unlike the blue 0.22 uF C69 and C73 |
| `3-owner-d70-pin15-to-ground.jpg` | the owner's board in 2015 (turned half round, the chip's notch on the right), twice enlarged: D70 (К555КП11), its pin 15 bent up off its pad, a bare wire to ground; the white wire along the same row is the factory's patch to pin 11 |
| `4-assembly-drawing-c70.jpg` | the assembly drawing 3.858.420 СБ (page 27, turned half round) at D80..D87: R39 between D80 and D85, C70 by D85 |
| `5-model-omega-left-edge.png` | the model: Omega booted, its 640 x 200 text at the left edge twelve times enlarged, the red lines at points 0, 16, 32, 48; above the owner's board (as the emulator), below the board as Э3 draws it |
| `6-e3-d21-r39-c70.png` | Э3 sheet 4 (page 21) at full resolution: D21 (КР556РТ12) with R39 and C70 on `БОРДС L` |
| `7-e3-d70-d71-d14.png` | Э3 sheet 4: D70 (К555КП11) with its inputs, D71 (К555ЛИ6, the load pulse) and D14.1 (К555ЛА3, D70's enable) |
| `8-timing-left-edge.png` | the race drawn with the datasheets' typical delays, in ns from the ТС15 H edge that steps D30 at the end of the load pulse |
| `9-radon17-video-chain.jpg` | Radon17's board (`photos/radon17-ns4`, 2024-10-20, turned half round), D70 К555КП11 with R24..R26 and the factory's white wire on pin 11, D71 К555ЛИ6, the registers К555ИР16 (9103), D57 КР1533ИР23, the video counters КР1533ИЕ10 (8910, 9001) and КР1533ТМ2 |
| `10-radon17-proms-d14.jpg` | the same board at D21 (СТРВ) and D13 (КДРВ), their factory paper labels, D14 КР1533ЛА3 (9002) beside them and the КР1533ИЕ10 counters above |
