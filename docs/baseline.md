# Baseline: the chain walked from the start

The last full walk of `docs/map.md`'s chain, step by step: the command,
the result, where the result is kept.  Nothing counts as passing that is
not in this file with its run.  The board run is the owner's (`vvv104`,
the model's default: D70.15 on GND); Э3's board is paused at stage 6b
(`docs/plan.md`).

**Walked 2026-11-05..06**, begun on commit a17d511 (trees: `rtl`
303e8f4, `firmware` 1a957b3 - neither changed by the walk); the emulator
`../ms0515` at 4325511.  What the walk found it fixed as it went, each in
a commit of its own; the corners and the tools' tests were run again on
the fixed tree.

| Step | State |
|---|---|
| 1. The netlist is consistent | pass, after a fix |
| 2. The netlist is the copper | pass: reproduces; Mick's last four differences read on Э3 |
| 3. The generated files are current | pass |
| 4. Chips and board, typical delays | pass, 73 of 73 |
| 5. The corners | pass, 73 of 73 at min and at max, after the tests were made to know a corner |
| 6. The real software on the board | ОСА and Omega to their prompts at typ, min and max: pass; on the core's drive too.  Found on the way and fixed: the controller's model ended a write a byte early and the bench's drive lost such writes; the boot runs made again on the fixed board, 17 of 17 pass |
| 7. The core | pass: built again after step 6's fix, timing met, +0.239 ns at 120 MHz |
| 8. The printed board | the owner's board placed and checked against the netlist.  Its routing stopped with 8 connections unrouted and **is not the board planned**: Freerouting ran the signals over the two plane layers - open |

## 1. The netlist is consistent

`python tools/netlist.py check`, `check vvv104`; `python -m unittest
discover -s tools/tests` with `$KICAD_CLI` set.

- **Found**: `netlist.py check` gave 14 errors, and had since 2026-10-21
  (eb8543f): that commit gave К555ИР16's pin 8 and КР1102АП15's pins their
  datasheets' names in `pinouts.csv` (`OC`; `W-S`, `1A`, `2A`, `1Y`, `2Y` -
  the board generator goes by `W-S`), while `nets.csv` keeps the labels
  Э3 draws (`OE`; `E`, `B`, `A`, `Y`, `YN`).  No net was wrong: the pin
  numbers agree.  It went unseen because no test ran the checker on the
  repository's own netlist.
- **Fixed** (0c7c524): `pinouts.csv` has a column `drawn` - Э3's label
  where it differs from the datasheet's name - and the checker takes
  either; two tests now run the checker on `netlist/` itself, as Э3 and
  as `vvv104`.
- **Result**: 0 errors both ways; 315 parts, 534 nets, 2129 pins on
  nets, 47 pinouts.  One warning, known: net `НАЧ H` has one pin
  (D97.12).  Tool tests: 112 of 112, none skipped.

## 2. The netlist is the copper

`tools/copper.py`, `footprints.py`, `mick_pdf.py`, `compare.py`,
`compare_mick.py`, `register.py`, `verify_traces.py` made again from
`$MS0515_DATA` (logs `build/step2-*.log`).  Their outputs are not kept in
the repository, so the check is their counts against `docs/findings.md`:

- copper: 3362 pads, 503 copper nets; footprints: 99 DIPs among the pads;
  the scans fitted to the traces at 2.0-2.1 px rms.
- `compare.py`: 1744 pins on pads, 0 placement problems; **8 joins** of
  the traces that Э3 keeps apart - the eight of the findings' table
  ("The copper against the schematic"), all closed there for Э3
  (2026-11-03); **171** schematic nets in pieces on the traces, as the
  findings say.
- `compare_mick.py`: 1331 signal pins in both readings, 1042 under the
  same name, **20 disagreements** - the findings' figures exactly.
- **Found**: four places of the 20 lines were written up nowhere in
  `docs/` (the findings said "settled so far"): `ЗАДЕРЖ L` (D83.26,
  D98.9), `ЗАП РЕГ H` (D79.3), `РВК1 H` (E3.1), `ЗЦ H` (D81.10).
- **Closed** (9c36306): each read on Э3 at full resolution - all four
  are as `nets.csv` has them; written into the findings with the crops
  in `docs/evidence/mick-differences`.  None of the 20 is open.

## 3. The generated files are current

`board.py check`, `ms7004_keys.py check`, `test_roms.py check`,
`kicad.py check`, `bom.py check`: all exit 0 - every generated file in
the repository is what its sources make.  `board.py` warns of four nets
nothing drives: `~D10.14`, `~D70.14`, `~D70.13`, `~D89.18`.

## 4. Chips and board, typical delays

`conan build . --build=missing -s compiler.cppstd=20`: 72 of 72 on
a17d511 (`build/baseline-typ.log`); with the walk's fixes and the one
test they added, `ctest --test-dir build/Release`: **73 of 73**
(`build/ctest-Release.log`).  The two tests of Э3's picture (`video:
Э3's board draws ...`) are marked may_fail and fail at typ (1 sample in
320 x 200, 1305 in 640 x 200), as stage 6b says.

## 5. The corners

`conan build . ... -o "&:corner=min"` / `max` on a17d511: 70 of 72 at
each (`build/baseline-min.log`, `build/baseline-max.log`).  None of the
failing tests had ever been run at a corner: the corners were last run
on 2026-11-01, the tests came on 2026-11-01..03.  Hence the rule in
`CLAUDE.md`: every corner when the board can change, a new test at every
corner on its first day.

- **min, `test_parts`: the two picture tests** - 4222 samples off the
  emulator's picture in 320 x 200, 100 in 640 x 200; Э3's board the same
  two numbers.  No change made it so: on the commit that brought the
  tests (421012f) they fail at min alike.  **Cause, measured**: the
  bench samples the picture at the rises of ТС15 H; the attributes'
  changes come 1 tick (min), 2 (typ), 3 (max) before the points', and at
  min the sampling tick falls between the two.  Single columns of
  samples at the edges and at every word's boundary; on a monitor, 8 to
  25 ns.  Not the board's fault (`docs/timing.md`, "Corners";
  `docs/evidence/corner-picture`).  **Fixed**: a board at a corner is
  held to the picture within one sample (`screen_compare.h`'s `slack`,
  with tests of its own); the typical board to the sample, as before.
- **min and max, `test_machine`**: its two comparisons set the core's
  machine (generated at the typical delays only) beside the bench's
  board at the corner - two different machines.  **Fixed**: the two are
  passed over at a corner, and say so.
- **max, `test_parts`, sound**: the node stood low with the way shut for
  4 ticks, the test allowing 3 - D93's delay (К555ЛА13: 19 ns typical, 2
  ticks; 30 ns at the most, 4).  **Fixed**: 4 at a corner.

With these, `ctest --test-dir build/min` and `build/max`: **73 of 73**
each (`build/ctest-min.log`, `build/ctest-max.log`).  Э3's two picture
tests (may_fail): at min both pass within one sample; at max 320 x 200
passes and 640 x 200 is 7 samples off - into the findings, the D70 case.

## 6. The real software on the board

`run_board 20000 FOLDER 30 disk=IMAGE keyboard [seed=N] [rom=...]`, 16
runs side by side, `build/baseline/boot/<run>/run.log` and frames.

- **A launch gone wrong, kept as a lesser result.**  The first nine runs
  were started without `keyboard`: ROM-B (and ROM-A) stopped at
  «Клавиатура не готова» and never read the disk
  (`build/baseline/<run>`).  What they do show, 20 s each: the ROM's
  self-test from random power-up states at typ (seeds 0-3) and max (1-3)
  with every fetch the emulator's; every frame compared the emulator's
  at max; at typ some frames 8 samples off at one place (400, 192: where
  the cursor stands - taken for the cursor's blink out of step, not
  looked into).
- **What a boot run checks.**  Up to the ROM's keyboard request the
  fetches are the emulator's; from there the emulator, given the ROM
  alone, goes another way (`fetch 1914937: the board at 173670, the
  emulator at 173640`; ОСА's at fetch 2685917) and the run is judged by
  its own lines.
- **ОСА** (`test_osa_games.dsk`, ROM-A), typ seeds 0, 1, 2, max, min:
  «ОСА Версия 1.0», `.SET TT QUIET`, the prompt, in 20 s - **pass**.
  Every DRAM read as written, but at max the cell 000:000 twice in the
  first 0.2 s (plan 7b's power-up cell).  Video requests lost: 0 or 1 at
  typ and min; 178 134 at max, the letters spoiled there (the design's
  margin, `docs/timing.md`).
- **Omega** (`test_vvv_system.dsk`), typ seeds 0-3, max 1-3, min 0-2:
  «ОМЕГА SJ(S) V05.04» and `.SET TT QUIET` in 20 s, every DRAM read as
  written; lost requests 0 or 1 at typ and min, about 80 800 at max.
  Its startup file's second line (`SET SL ON`) was not through: no
  prompt yet.
- **Omega to 30 s** (`build/baseline/boot30`, the same ten runs): the
  banner, `.SET TT QUIET` (at 17.5 s), the prompt (at 23 s) and from
  there only the cursor's blink - **pass** at typ, min and max.  It is
  the emulator's screen at its end: `ms0515-cli --no-config --rom
  ms0515-romb.rom --disk0-side0 IMAGE --frames 1500 --screenshot FILE`
  shows the same three lines, and the same at 1000 and at 2500 frames
  (compared by eye: the emulator is not followed past the ROM's keyboard
  request).  Every DRAM read as written in all ten; lost requests 0 or 1
  of 28 125 001 at typ and min, some 246 450 at max, the letters spoiled
  there as ОСА's.  Not shown: that `SET SL ON` took effect - the line is
  not echoed after `SET TT QUIET`, and only a key typed would tell.
- **The core's drive** (`drive=logic`, Omega, typ): 2420 blocks read, 4
  written, the two images the same at the end - plan 7c's figures.  The
  two drives' lines differed in 7322 stretches, the longest 120 ticks,
  the first at 0 s; `docs/bench.md` said "only while a track is loaded
  after a step, some thousand ticks each".  The description was wrong,
  not the count: while the logic loads a track the bench's drive gives
  its flux pulses and the logic none, so a stretch is one pulse (120
  ticks) and a load a run of them - `docs/bench.md` and
  `logic_drive.h` now say so.
- **The core's drive against the bench's, to 30 s**
  (`build/baseline/logic`, `boot30/omega-typ-logic-drive`): Omega
  reaches its prompt as on the bench's drive (its kept frames change
  where and when that run's do), 2740 blocks read, 8 written - and **the
  two drives' images differed at the end**, where at 20 s, 4 blocks
  written, they were the same; and 42 ticks of their lines differed
  later than 3 ms after a head's move, where the design has none.
- **Found** with two runs printing every write (`build/baseline/logic2`,
  `logic3`; the printing not in the tree) and `test_mc5305`.  Two faults,
  neither the core's drive's:
  - *The controller's model* (`rtl/chips/kr1818vg93.sv`) let WG fall as
    the FF byte after a sector's CRC began, not as it ended: the gate on
    for 530 bytes (measured on the board: 2 035 321 ticks) where the
    datasheet's sequence, which `docs/vg93.md` gives, is 531.
  - *The bench's drive* (`sim/board/drive.h`) made the cells it reads a
    write by from ДЗП's falls alone, so the cells after the last fall
    never came: with nothing written after the CRC, a field whose CRC
    ends in a 0 was never whole and **the write was lost**.  The logic's
    drive counts cells by time and took it.
  On the board: the writes at 14.704 and 14.743 s (blocks 143, 145)
  change nothing in either image; those at 18.202 and 18.422 s (168,
  159) both drives take; those at 21.802 and 22.022 s (168, 159 again)
  the logic's takes and the bench's does not - two bytes apart in each.
  The 42 ticks are 21 after each of the writes at 18.2 and 18.4 s: the
  first cell after the CRC is a clock cell that depends on the CRC's
  last bit, the bench's drive had the new sector there at once and the
  logic the old one still, and the gate went off 99 ticks into that
  120-tick pulse.
- **Fixed**: the controller's model holds WG for the FF byte (a test of
  the gate's length, 531 bytes); the bench's drive counts the cells
  after the last fall up to the write's end (a test of a field with
  nothing after its CRC, its last bit 0 or 1; and the two drives beside
  each other on such a field in `test_mc5305`).  `test_mc5305` also
  writes each of a track's ten sectors by its place in every layout on a
  single-sided image, four sectors one after another in a turn, a sector
  before its place, and compares the lines while a sector is written.
  The run's report now counts the ticks within four bytes after a
  write's end apart, and lists the blocks where the images differ.
  `ctest` at typ, min and max after the fix: **73 of 73** each
  (`build/ctest-Release.log`, `ctest-min.log`, `ctest-max.log`).
- **What it means for the runs above.**  Every boot run of this walk was
  made with the controller's model a byte short and, but for the two
  `drive=logic` ones, on the bench's drive that lost such writes: the
  system's later writes of its own disk did not reach the image.  They
  reached their prompts all the same (the blocks were not read again
  within the runs), but the runs are of the board before the fix.
- **Run again after it** (`build/baseline/after/<run>`, 2026-11-06
  08:30 to 12:25, the tree of 26b39da, 17 runs side by side;
  `build/baseline-after.log`, `baseline-after-seeds.log`) - **pass**,
  all seventeen:
  - *ОСА, 20 s*: typ seeds 0, 1, 2, min, max (seed 1), and typ on the
    core's drive - «ОСА Версия 1.0», `.SET TT QUIET`, the prompt in all
    six; every DRAM read as written (`0 differ`); lost requests 0 or 1
    at typ and min, 178 210 at max, the letters spoiled there as before.
    The typ and min runs end on the very fetch counts of the runs before
    the fix: ОСА writes nothing in its 20 s (`0 written`).
  - *Omega, 30 s*: typ seeds 0-3, min 0-2, max 1-3, and typ on the
    core's drive - the banner, `.SET TT QUIET`, the prompt in all
    eleven; every DRAM read as written; lost requests 0 or 1 at typ and
    min; 246 382, 246 462 and 246 417 at max, the letters spoiled there.
    The fetch counts are a few off the runs' before the fix (min, seed
    0: 11 658 515, was 11 658 506) - a write a byte longer.
  - *The core's drive beside the bench's* (`omega-typ-logic-drive`):
    2740 blocks read, 8 written, **the two images the same**; 0 ticks
    of their lines apart within four bytes after a write's end, 0 later
    than 3 ms after a head's move - the lines differ only while a track
    loads (8368 stretches, none longer than a pulse, 120 ticks).  Neither image differs from the one
    mounted, and that is what a drive that took all eight writes ends
    with: the system's later writes put back what its earlier ones
    changed (the run before the fix that printed every write,
    `build/baseline/logic3`: after them the logic's blocks 159 and 168
    as mounted, the bench's 3 and 2 bytes off).  Its kept frames change
    at the frames the bench drive's run's do, and the last ones are the
    same sample for sample.  ОСА's (`osa-typ-logic-drive`): 1330 blocks
    read, none written, the images the same, no tick apart late.
  - `omega-typ` left a snapshot of the machine at the prompt
    (`build/baseline/after/omega-typ-24s.snap`, at 24 s).

## 7. The core

`quartus_sh -t boards/de10nano/core.tcl`, reports in
`build/mister/output_files/`.  Built twice: on a17d511
(`build/baseline-core.log`), and again on 26b39da
(`build/baseline-core2.log`, 2026-11-06, 20 minutes) after step 6
changed `rtl/chips/kr1818vg93.sv` (a state more in the write's
sequence).  The figures are the second build's, the first's in
brackets.

- Resources (`ms0515.fit.summary`): 13 557 of 41 910 ALMs, 32 %
  (13 963, 33 %); 1 643 857 of 5 662 720 block memory bits (29 %), 214
  of 553 RAM blocks, 33 of 112 DSP blocks (29 %) - as before.
- Timing (`ms0515.sta.rpt`, the template's single model): every domain
  met.  The 120 MHz domain (`emu|pll ... general[0]`): setup slack
  +0.239 ns, Fmax 123.55 MHz, hold +0.344 ns (+0.647 ns, 130.11 MHz,
  +0.350 ns).  It is now the least setup slack of all (before: +0.469
  ns, HDMI's PLL).  The change is one state in one chip, and the 0.4 ns
  between the two builds is not its path: the fifteen worst paths of
  the second build (`report_timing`, `build/mister/worst.txt`) are the
  key translator's (`ms7004_keys`: `ev_q` and the 8035's port to
  `found`, +0.239 and +0.291 ns) and the drive's writer's (`layout_q`
  to `there`, from +0.407 ns), none in the controller.  The fitter
  placed the design anew; its margin at 120 MHz is a few tenths of a
  nanosecond either way, as plan 7d's history shows.
- Not done: the post-fit netlist on the bench (plan 7d).

## 8. The printed board

The owner's board first (`docs/plan.md` 8d).

- `tools/pcb.py vvv104`: 312 footprints, 536 nets,
  `kicad/ns4-vvv104.kicad_pcb` - placed, no wires.  `pcb.py check
  vvv104`: 0 differences from the netlist; `pcb.py check` (Э3's board,
  placed the same way): 0.  The two boards' placements compared part by
  part: none differs.
- The placed board through KiCad's DRC before any wire (`kicad-cli pcb
  drc --severity-all`, `build/drc/vvv104-placed.json`): no error; 59
  warnings, all the silkscreen's (55 overlaps, 2 clipped by the board's
  edge at XS2, 2 over copper); 499 unconnected items, there being no
  wires.  So the placement brings no clearance or courtyard fault into
  the routing.
- `tools/route.py vvv104 start`, `run 2 5`: from the empty board, 1258
  connections; unrouted after each chunk: 382, 238, 162, 116, 81, 64, 50
  (2026-11-06 06:30), 25, 20, 18, 16, 14, 11, 9, 8 (10:08), 0 violations
  by Freerouting's count throughout (`build/route-vvv104/progress.txt`).
  It **stopped at 8**: five chunks of 2 passes and then three of 12
  (`run 12 3`) gained nothing.  The eight: `Д15 H`, `ДЧТ L`, `ТС4 H`,
  `Д05 H`, `МК02 H`, `МК07 H`, `~VT1.C2`, `Р15 H`, one connection each.
- **Found when that state was put on the board** (`route.py vvv104
  finish`; the board kept as
  `build/route-vvv104/ns4-vvv104-routed8.kicad_pcb`, KiCad's DRC of it
  `build/drc/vvv104-routed8.json`; the tracked board put back as placed,
  nothing of the routing committed).  `pcb.py check vvv104`: 0
  differences from the netlist - the wires join the right pins.  But it
  is not the board `docs/plan.md` 8 describes (signals outside, ground
  and +5 V as planes inside):
  - *The signals run over the plane layers.*  Track on In1.Cu 25.2 m
    (5396 segments), on In2.Cu 30.3 m (5274), on F.Cu 11.5 m (2506), on
    B.Cu 12.0 m (3005).  The planes are cut to pieces by it: 242 `GND`
    and 181 `+5V` items unconnected, 114 thermal reliefs starved.
    Cause: the design handed to Freerouting (`build/route-vvv104/ns4.dsn`)
    names all four layers `(type signal)` - `tools/pcb.py` draws the
    planes as zones and leaves the layers' type as it was; Freerouting
    logs that it takes the planes' nets as routed by the plane, and
    routes signals through the same layers.  Its "0 violations" is by
    that design.
  - *Tracks narrower than the rule.*  3740 segments 0.1874 mm wide, 50
    of 0.15 and 33 of 0.125, where the rule given is 0.25 mm and the
    board's least 0.2 (DRC: 199 `track_width` errors).  Where the widths
    come from - not looked into.
  - *The edge.*  40 `copper_edge_clearance` errors, 0.35 to 0.47 mm
    where 0.5 is set.
  - *It was so from the first routing*: the board routed on 2026-11-04
    (f83c375's `kicad/ns4.kicad_pcb`) has 22.7 m of track on In1.Cu and
    28.0 m on In2.Cu, and the same narrow widths.  No routed board had
    been through KiCad's DRC in this walk until now; the placed one had.
- **A probe of the planned stack** (`build/route-probe2`, nothing
  tracked): the same empty design with In1.Cu and In2.Cu named `(type
  power)`, so that no signal may use them, 12 passes.  Its first pass
  left 592 of 1258 unrouted (the four-layer routing's first pass: 528).
  **Running**; its end goes here.
- Not done, and not to be done on this routing: DRC to no errors, Э3's
  board from this one.  What the board's stack is to be is the owner's
  to say once the probe has ended (open list, 5).

## Open after this walk

1. Omega's startup to its end (step 6) - done, pass.  Left, not begun
   (queued for the owner's word): a command typed at the prompt
   (`keys=FILE`, the МС 7004's codes by the key's place) - it would
   show the keyboard under the system, a program loaded from the disk,
   and whether `SET SL ON` took effect.  The snapshot at the prompt
   is there (`build/baseline/after/omega-typ-24s.snap`), so the run is
   of the seconds typed, not of the boot.
2. The core's drive against the bench's (step 6): found and fixed -
   the controller's model ended a sector's write a byte early and the
   bench's drive then lost the write; the core's drive was right.  The
   core is built again (step 7), the boot runs made again on the fixed
   board: 17 of 17 pass, the two drives' images the same after Omega's
   eight writes.  Closed.
3. The cursor's 8 samples in the self-test's frames at typ (step 6) -
   taken for the blink's phase, not shown.
4. The post-fit netlist on the bench (step 7, plan 7d).  Tried on
   2026-11-06 in the build tree (`build/postfit`, nothing of it in the
   repository), and it does not work as tried:
   - the machine alone (`rtl/ms0515.sv`, no `sys/`) goes through Quartus
     - 6 021 ALMs, 1 259 600 block memory bits, no DSP block - and the
     EDA netlist writer gives its post-fit netlist as Verilog: 9 912
     `cyclonev_lcell_comb`, 5 237 `dffeas`, 158 `cyclonev_ram_block`, the
     pins' buffers and one `cyclonev_clkena`;
   - Quartus's simulation models are open for those cells but the last
     (`quartus/eda/sim_lib`: `cyclonev_atoms.v`, `altera_primitives.v`,
     `altera_lnsim.sv`); the clock's enable cell, encrypted, needs a
     stand-in of two lines;
   - Verilator 5.044 stops on the netlist with an internal error
     (`V3Gate.cpp:962`) unless its gate optimisation is off (`-fno-gate`),
     and wants `--no-timing` for the models' delays;
   - so built, beside the RTL machine with the same inputs, the model
     runs some 80 master ticks a second and its outputs differ from the
     RTL's at every tick from the first (`ce_pix` stuck high, `kbd_tx`,
     `kbd_bell`, `speaker`): Quartus's models do not work without their
     delays, not a finding about the fit.
   What it would take: models of our own for the three cells (the
   flip-flop, the look-up table, the RAM block), which Verilator can
   optimise.  The owner's to decide; until then the fit is trusted as
   every MiSTer core trusts it, and 7e - a board - is its check.
5. The printed board (step 8): the routing as made is not the board
   planned - Freerouting was given the plane layers as signal layers
   and ran more than half the track over them; it also left 8
   connections, narrow tracks and the edge's clearance.  First
   `tools/pcb.py` has to hand the inner layers over as planes; whether
   the board then routes on its two outer layers is being probed.  If
   it does not, the stack is the owner's to choose (two more signal
   layers, or other rules).  After that: DRC, the silkscreen's
   warnings, Э3's board from it.
6. Э3's board, stage 6b - paused.

## Said from memory before this walk, and wrong

Kept as the reason for the rule "a state only from the files":

- "Waiting for Frolov's answer": nobody asked him anything; he is the
  author of the 2009 museum photos.  The question of
  `docs/evidence/d70-left-edge` is asked of the owners of working
  machines, on the forum, and has no answer.
- "Stage 8b not begun": done on 2026-11-03 as test ROMs (`docs/plan.md`
  8b now says so).
- "At the min corner the screen is clean" (plan 7b) read as covering the
  picture tests: it was of ROM-B's screens, before those tests were.
- The comparison outputs (`netlist/copper.json`, `compare.txt`, ...)
  "kept in the repository": they are in `.gitignore`.
- The first boot runs of this walk, launched without `keyboard` from a
  command remembered, not written anywhere: two hours of the ROM's
  self-test.  The command is in `docs/map.md` now.
- "The 42 ticks: the board's controller writes its field a byte and a
  part before the layout's place" (commit 4761fe4, as a likely cause,
  and for an hour as confirmed in the working tree): a guess that fitted
  one number.  The field is at its place and was a byte short; the ticks
  are the first cell after the CRC (step 6).
- "Omega's screen at its end, per the emulator: the banner and a cursor
  under it": that picture was the emulator's on ROM-A, its default, where
  Omega stops after its banner.  The board runs Omega on ROM-B; on ROM-B
  the emulator ends as the board does (step 6).
