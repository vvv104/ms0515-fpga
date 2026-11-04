# The left edge of a 640 x 200 line: D70, БОРДС L and C70 - an open question

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
- The first word of the frame's top line comes from the frame before:
  through the top border's last line `БОРДК L` keeps D70 off all the
  line, so nothing is loaded at its right edge.  Logic, not a race -
  the margin is microseconds.

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

## 4. Where it stands, what is open

If hundreds of serial boards worked and only the odd one did not (as
the owner's), the typical board won this race with room to spare.  By
the data sheets it loses by ~40 ns.  So something here is wrong by tens
of ns - and the delays are not to be bent to hide it.

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
