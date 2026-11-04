# The left edge of a 640 x 200 line: D70, БОРДС L and C70 - an open question

Gathered for the owners of working MS 0515 machines (zx-pk.ru), so that
everything the question rests on can be looked at in one place.  The
whole case is told in [findings.md](../../findings.md), section «The
factory wire patches restore Э3», «What it does, on the model».

## What is being solved

The board НС4 is rebuilt from 3.858.420 Э3 gate by gate, as a model
with each chip's datasheet delays.  At one place the picture depends on
a race of edges, and we cannot see how the serial machines won it.

- The points and attributes are shifted out by the К555ИР16 registers
  D49, D50, D55, D56.  Their clocks and their load/shift control (MODE)
  come through the multiplexer D70 (К555КП11) as `ВК2`, `ВК3`, `ВК4`
  (picture 7).
- D70 is off in the border: its enable `EO` (pin 15) is D14.1 = NAND
  (`БОРДС L`, `БОРДК L`).  In the border its outputs float, the clocks
  stop, and the registers hold the word they loaded at the right edge
  of the line before.
- `БОРДС L` is an output of the PROM D21 (КР556РТ12, open collector),
  pulled up by R39 (1 kOhm) and loaded by C70 (100 pF) to ground
  (picture 6).  So the border's end - `БОРДС L` rising - comes slowly,
  its start quickly.

In 640 x 200 a point is ~67 ns (15 MHz).  For the line's first word to
come out in place, D70 has to be on no later than ~45-65 ns after the
step of the counter D30 that ends the load pulse: then the clock's next
fall shifts.  Later, and that fall loads instead - the first 16 points
of every line come out one point to the right, the 16th lost (or, by
the right edge's own race, the first point doubled).

The path to D70's enable: D30 (К555ИЕ10) -> D21 (КР556РТ12) -> `БОРДС
L` rising through R39 / C70 -> D14.1 (К555ЛА3) -> D70's enable
(К555КП11).  With the datasheets' typical delays that is ~100-110 ns
(picture 8): the race is lost by a wide margin, and the shift would
show on every machine.  On the model it does (picture 5: Omega's text,
«ОМ» at the left edge one point to the right).

On the owner's board (made 07.1991) this very place carries two
changes: C70 bitten off - its leads' stubs left in the pads (picture
1) - and D70's pin 15 bent up off its pad and tied by a bare wire to
ground (D57.10), D70 always on (picture 3).  Both make exactly this edge
faster: it looks like a repair of exactly this fault.  Frolov's board
(leningrad.su, 2009) has C70 fitted and no change at D70 (picture 2).
The serial machines, by all accounts, drew 640 x 200 right.

## What it may be

1. We read the board wrong: C70 is not on `БОРДС L` on the copper (on
   `БОРДК L`, or elsewhere), the PROMs D21 / D13 swapped, or D70's
   enable wired otherwise than Э3 draws it.
2. The real chips were well faster than the data sheets - above all the
   КР556РТ12, for which we have no figures of its own (the Signetics
   82S136's are used: 40 ns typical, 60 maximum).
3. The fault was there on part of the boards, but not seen: if it takes
   only the first column of points, a font's first column is usually
   blank.
4. Something we do not see.

## What would settle it

1. A close photo of a working machine's screen in 640 x 200 at the left
   edge, with points at the very edge (a small program drawing vertical
   lines in columns 0-3 and 15-17 can be provided).
2. A continuity check on a board: where C70 (К10-17, 100 pF, by D85 on
   the D84 side) goes - to D21 pin 14 (`БОРДС L`), to D13 pin 14 (`БОРДК
   L`), elsewhere; whether it is fitted at all; any changes at D70 pin
   15 and at R39.
3. The КР556РТ12's own figures (address access time, typical and
   maximum) from a handbook.

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
