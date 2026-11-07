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
| 6. The real software on the board | ОСА and Omega to their prompts at typ, min and max: pass; on the core's drive too.  Found on the way and fixed: the controller's model ended a write a byte early and the bench's drive lost such writes; the boot runs made again on the fixed board, 17 of 17 pass.  Then games (BIRDS, Saboteur 2) found the КР580ВВ51А's model wrong twice about TxEN - a character written before TxEN went off was held, and one written with TxEN off never left the buffer - so that both stood still at their first instructions; fixed; the boots on the board so fixed, 17 of 17 pass; on it both games run at typ, min and max - BIRDS to its field with its sound, Saboteur 2 through its menu and briefing to the jump, the landing and its sound, by the keypad and by the joystick.  Saboteur 2 takes 20 s to load there, a block a turn of the diskette - open (9) |
| 7. The core | pass: built again after each of step 6's fixes, timing met, +0.630 ns at 120 MHz after the last |
| 8. The printed board | the owner's board placed and checked against the netlist.  Its routing stopped with 8 connections unrouted and **is not the board planned**: Freerouting ran the signals over the two plane layers.  A probe with those layers closed to signals left 140 of 1258 connections after 12 passes - open |

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
  the cursor stands - taken then for the cursor's blink out of step; it
  is not that, open list 3).
- **What a boot run checks.**  Up to the ROM's keyboard request the
  fetches are the emulator's; from there the emulator, given the ROM
  alone, goes another way (`fetch 1914937: the board at 173670, the
  emulator at 173640`; ОСА's at fetch 2685917) and the run is judged by
  its own lines.
- **ОСА** (`test_osa_games.dsk`, ROM-A), typ seeds 0, 1, 2, max, min:
  «ОСА Версия 1.0», `.SET TT QUIET`, the prompt, in 20 s - **pass**.
  Every DRAM read as written, but at max the cell 000:000 twice in the
  first 0.2 s (plan 7b's power-up cell - the bench's watch's own, not
  the board's: "Games" below).  Video requests lost: 0 or 1 at
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
    loads (8368 stretches, none longer than a pulse, 120 ticks).
    Neither image differs from the one mounted, and that is what a
    drive that took all eight writes ends
    with: the system's later writes put back what its earlier ones
    changed (the run before the fix that printed every write,
    `build/baseline/logic3`: after them the logic's blocks 159 and 168
    as mounted, the bench's 3 and 2 bytes off).  Its kept frames change
    at the frames the bench drive's run's do, and the last ones are the
    same sample for sample.  ОСА's (`osa-typ-logic-drive`): 1330 blocks
    read, none written, the images the same, no tick apart late.
  - `omega-typ` left a snapshot of the machine at the prompt
    (`build/baseline/after/omega-typ-24s.snap`, at 24 s).
- **Games** (the owner's word, 2026-11-06: a harder test than a boot -
  BIRDS, which sounds at once, and the 1991 Saboteur 2, which works
  through the memory dispatcher's banks and reads the joystick).  The
  bench was given the speaker's node as `sound.wav` and the joystick
  (`joy=FILE`), and a diskette is made for it - DEC RT-11 SJ with the two
  games (`tools/games_disk.py`; `docs/bench.md`, "Games"; 6716c7b).
  - *The diskette booted* at typ, min and max (`build/games/boot-<corner>`,
    24 s, the tree of 6716c7b): «RT-11SJ V05.04», `.SET TT QUIET`, the
    prompt at 13.5 s in all three.  The speaker's node gives the ROM's
    self-test's five notes - 331.1, 417.3, 496.2, 662.3 and 331.1 Hz,
    0.35 s each, from 2.94 s (typ) - the same in the three corners.
    Every DRAM read
    as written at typ and min.  At max 900 reads of the cell 000:000
    differ (000000 read, 177605 written) - the bench's watch's own, not
    the board's (the next item).  Lost requests: 0 at typ and min,
    337 952 at max.
  - *The cell 000:000 at max: the watch's shadow was the one off.*  The
    watch (`sim/board/dram_watch.h`) had no time in its lines; given
    one (each read that differs with its time, whose it is and when the
    cell was written; the first and the last kept), the max corner's
    first 0.4 s (`build/cell0/short`) says: the "write" of 177605 is at
    0.000000 s, all 900 reads are the processor's, none the video's,
    the first at 0.000374 s, the last at 0.207377 s - a microsecond
    before ROM-B's test first writes the cell (0.207378 s).  From an
    unseeded power-up at max -RAS, -CAS and both -WE stand low at the
    first tick with 177605 on МК; the watch began with its "strobes
    before" high and took that for a falling edge and a write.  No
    КР565РУ5 does: the chips' own "before" start low (the cell reads
    000000).  The reads are the processor's of address 0 before the
    board's reset has started it at the ROM (fetches at 000000 and
    140004 from 24.3 ms, `build/cell0/f`; the first fetch counted comes
    between 60 and 70 ms) and its test's; with ROM-A the cell is read
    twice in that time, hence ОСА's two.  Fixed in the
    bench: strobes that stand low at the watch's first tick are no
    edges (`test_dram_watch`, seen failing with the very line of the
    run).  The same 0.4 s after it: 0 differ, one write and 900 checked
    reads fewer (`build/cell0/short-fixed`).  The board is not touched.
  - *Found: the model of КР580ВВ51А held a character.*  `R BIRDS` and
    `R SABOT2` typed at the prompt from the snapshot at 14 s (`keys=`,
    `build/games/birds-<corner>`, `sabot-load-<corner>`): both load;
    Saboteur 2's title picture stands from 16.5 s in the three corners
    (max's the very samples of typ's); then neither game goes on.
    BIRDS sits at 001012 from 16.55 s (`irq`, `build/games/birds-diag`):
    its first instructions wait for TxRDY in the keyboard USART's status
    (`BIT #1,@#177442`), send the keyboard 231 twice and take TxEN off
    (command 064); Saboteur 2's `SABOT2.DAT` begins with the same lines.
    TxRDY never came, because ROM-B had left a character in D22's
    buffer: at 4.8158 s it writes the command 007, the data 021, the
    data 220 and, 10 µs after it, the command 006 - TxEN off with 220
    still in the buffer (`watch=0177440-0177463`,
    `build/games/kbd-diag-a`; the keyboard heard 021 and no 220).  The
    same at 2.78 s, where the 220 left only at 4.808 s, when TxEN came
    back.  The 8251A sends what was written before a Tx Disable
    (`docs/vv51.md` §7, DS p.2-2, p.2-4 - written down there and not
    done by the model, which began a character only with TxEN on).  The
    emulator's USART is always ready, so both games run there.
  - *Fixed*: `rtl/chips/kr580vv51.sv` - a character that waited in the
    buffer while the transmitter was on is still sent after TxEN or
    -CTS goes off; one written with it off waits.  Test first: two cases
    of `test_kr580vv51` (TxEN, -CTS), seen failing.  `ctest` at typ, min
    and max after it: **75 of 75** each (the two bench tests of 6716c7b
    added to the 73).  On the fixed board the keyboard hears 247, 021,
    220 one after another at 2.78 s, and 253, 021, 220 at 4.81 s
    (`build/games2/boot-<corner>`, fa63fda's tree, to 16 s: the prompt
    in the three corners).
  - *The same chip, a second time.*  On the board so fixed the games
    still stood (`build/games2/birds-<corner>`, `sabot-load-<corner>`):
    BIRDS writes its first 231 at 16.550372 s and stands at 001030, its
    second wait for TxRDY (`build/games2/birds-diag`).  ROM-B turns TxEN
    on only around its own writes and leaves the command at 006, TxEN
    off; the character written then never left the buffer.  And 006 is
    what a program finds under Omega too (the emulator's states at the
    prompts: Omega and DEC RT-11 on ROM-B 006, ОСА on ROM-A 047) - the
    system the games were written under, so on the machine TxRDY comes
    back after a write with TxEN off.  The datasheet does not say what
    becomes of such a character.  Chosen: it leaves the buffer for the
    shift register as soon as that is free and begins there when TxEN
    comes on; TxEMPTY stays high meanwhile (`docs/vv51.md` §7; what is
    left open is §13, 13: kept, dropped or sent at once - only if sent
    at once does the keyboard hear the games' 231, "no key click",
    while the game runs; asked of the owner).  Test first: a case of
    `test_kr580vv51`, seen failing.
    `ctest` at typ, min and max after it: **76 of 76** each (with
    `test_dram_watch`).
  - *What it means for the runs above.*  Every boot run of this walk
    was made with the keyboard's commands late or lost - nothing in the
    bench's keyboard stub acts on them, but the processor's path is not
    the same: the fetch counts of the games diskette's boot before and
    after the fix part at 2.79 s by a fetch or three
    (`build/games/boot-typ`, `build/games2/boot-typ`).  In the MiSTer
    core the МС 7004's own model was the one not hearing them.  So the
    seventeen boots were made once more, on the board after both
    fixes (`build/baseline/after3`; those begun after the first,
    `after2`, were stopped), with the diskette's boot and the games
    (`build/games3`).
  - *The seventeen boots on the board after both fixes*
    (`build/baseline/after3/<run>`, 2026-11-06 17:44 to 21:08, the tree
    of 127a404; `build/baseline-after3.log`,
    `baseline-after3-seeds.log`) - **pass**, all seventeen, the runs of
    "Run again after it" above one for one: the banners, `.SET TT
    QUIET` and the prompt in each (`build/baseline/after3/last.png`,
    the last kept frame of each; the letters spoiled at max as before);
    every DRAM read as written (`0 differ`, at max too - the watch's
    start is mended); lost requests 0 or 1 at typ and min, at max
    178 210 (ОСА) and 246 366, 246 467, 246 420 (Omega).  The keyboard
    hears ROM's commands whole in every run: 247, 021, 220 at 2.75 to
    2.80 s, and 253, 021, 220 at 4.78 to 4.82 s under ROM-B, at 6.64 to
    6.68 s under ROM-A.  ОСА's six runs end on the very fetch counts of
    the runs before the USART's fixes, and their last frames are the
    same file for file: ROM-A never waits for the transmitter.  Omega's
    eleven are 140 to 200 fetches short of theirs (typ, seed 0:
    11 633 813, was 11 633 968); the last frames of the five at typ are
    the same, those at min and max not sample for sample.  The core's
    drive beside the bench's as before: Omega 2740 blocks read, 8
    written, ОСА 1330 read, none written, the two images the same and
    as mounted.
  - *The games diskette's boot on that board* (`build/games3/boot-<corner>`,
    15 s): the prompt in the three corners
    (`build/games3/view/boot840.png`), the five notes at the times
    above within 0.1 ms, every DRAM read as written in all three, lost
    requests 0, 0 and 126 714.
  - *BIRDS on it* - **runs**, typ, min and max (`build/games3/birds-<corner>`
    14 to 23 s, `birds2-` to 30 s, `birds3-` to 35 s, each from the
    snapshot the one before left).  `R BIRDS` typed from 14.2 s, Return
    at 14.62 s.  The screen fills with coloured squares by 16.97 s and
    they thin out (`build/games3/view/first.png`, `birds.png`); the
    title - «FIREBIRD», «1990 LWOW SOFT», the scores, «PRESS ANY KEY TO
    START» - stands at 25.46 s at typ and min and by 26.46 s at max
    (`birds2.png`); Space at 27.5 s brings the curtain of coloured bars,
    and from 31 s the field: the line of the score, the lives and
    «WAVE01», a flock that flies and drops, the ship, a hit's white
    flash, the lives going from three to two (`birds3.png`; the
    emulator's field: the same line and sprites).  The flocks are not
    the same in the three corners: the game's chances follow its timing.
    Sound from 16.6306 s (typ and min; 16.6373 s at max): 43 065, 43 602
    and 43 605 edges of the speaker's node to 23 s, the commonest gap
    between them 90 us in all three, none while the title stands, then
    again from the key (`birds*-<corner>/sound.wav`; the bench's "tones"
    are stretches of evenly spaced edges, and a noise has few).  Every
    DRAM read as written in the nine runs; lost requests at max 294 886,
    272 599 and 57 436, and their marks in max's frames as in the boots.
  - *Saboteur 2 on it* - **runs**, typ, min and max, and **loads
    slowly**.  `R SABOT2` typed from 14.2 s, Return at 14.68 s
    (`build/games3/sabot-load-<corner>` to 25 s, `sabot-load2-` to 40 s).
    The title picture stands from 16.47 s; the diskette is read until
    36.83 s (typ and min; 37.23 s at max); the picture of the menu comes
    at 39.43 s and the menu - K KEYBOARD, P PROTEK, J KEMPSTON, R
    REDEFINE KEYS, M ALTER MISSION, S START GAME - by 39.93 s (max half
    a second later; `build/games3/view/sabot-load2.png`).  No sound, and
    every DRAM read as written.  Then from the snapshot at 40 s, twice:
    - *the keypad* (`sabot-kbd-<corner>`, to 53.5 s): Space 40.8 s, S
      41.6 s - the yellow «MISSION BRIEFING ... PRESS ANY KEY TO
      CONTINUE»; Space 43.2 s - the game: the moon, the hang-glider
      flying in, the panel (PAY, the strength, TIMER 998, SHURIKEN
      HELD, NOTHING NEAR); keypad 5 at 46.1 s - the figure lets go and
      falls from screen to screen, lands crouching on the girder at 50.2 s
      (50.4 s at max) and stands, the strength shorter; keypad 4 at
      51.7 s turns it left (`build/games3/view/sabot-kbd.png`,
      `sabot-kbd-figure.png`).  The game's first sound is the landing's,
      as the owner said: 200 edges, 50.229 to 50.339 s at typ, 50.212 to
      50.301 s at min, 50.386 to 50.477 s at max.
    - *the joystick* (`sabot-joy-<corner>`, to 55 s; J at 41.6 s, S,
      Space at 44.0 s): fire held from 46.9 s is the jump, the landing's
      sound from 51.03 s (51.01 s min, 51.18 s max); then right, left,
      down and up held in turn from 51.5 s, and the figure runs right,
      runs back left, crouches, leaps (`build/games3/view/sabot-joy.png`,
      `sabot-joy-figure.png`);
      260, 260 and 240 edges of sound to 53.4 s.  The cell watched
      (`watch=041460-041461`) takes 020 at 47.0006 s, 001 eight times
      from 51.579 s, 002 nine times from 52.535 s, 004 three times from
      53.573 s and 010 at 54.043 s (typ) - the lines held, each within a
      tenth of a second of its being set.  It is not a variable of the game's, as
      `docs/bench.md` had it: the game draws its control bytes into a
      column of screen cells through the video window (`SABOT2.DAT`
      004354 on), and 041460 there is where the third lands, 041461 its
      attribute.  During the fall the same cell takes other values (typ
      0120, 0370, 0170; max 001, 007, 0100) with no line held - the
      game's own: what it reads off the port (001464: `MOV @#177542,R0`,
      `COM`, `BIC #177740`) has five bits.
    Every DRAM read as written in the twelve runs; lost requests at max
    67 505 and 194 969 (the loading), 323 934 and 348 383.
  - *Why it loads for 20 s.*  The loader reads `SABOT2.DAT` (85 blocks)
    a block a request and works on each for 23 ms (6400 of its 7700
    fetches between two requests are its own and ROM-B's: 4608 at
    0021xx, 1792 at 1670xx).  The handler
    (the diskette's `DZ.SYS`, the DEC kit's; taken apart from the
    emulator's state at the prompt, 153520 on) begins every request with
    Read Address (command 300), sets the track and the sector and gives
    Read Sector (200) - `watch=0177600-0177677`, `build/games3/sabot-fdc`:
    300 at 17.013447 s, 200 at 17.032040 s, the 512 bytes taken from
    17.232940 to 17.249294 s, the next 300 at 17.272391 s, the sectors 8,
    10, 2, 4 of track 19.  The blocks lie two sectors apart (the
    driver's 2:1 interleave), the next one's ID some 21 ms after the
    last one's end; by the time Read Address has read an ID and Read Sector
    is given, it has passed, and the block costs a turn: **240 ms a
    block**, 85 of them 20.4 s.  Nothing stands still: the processor
    waits in the handler's loop for DRQ (153574) between them.  Omega's
    own `DZ.SYS` begins a request the same way (from the emulator's
    state at Omega's prompt: 153054 `JSR R0,153626` with 300 in line,
    then 153134 `MOVB @#177644,@#177642`).  The emulator shows the menu
    7 to 8 s after Return: its controller finds any sector 5 ms after
    the command (`TYPE2_SEARCH_CYCLES` of its `floppy.c`), with no
    turning diskette under it.  Which the machine did is not in the
    files; the same game from Omega's own diskette is being run on the
    board (open list, 9).
  - *The bench's line after a snapshot*, "the keyboard heard 220" at the
    load's time, is the bench's: its count of codes heard starts at zero
    and the stub's list comes with the snapshot.  The games' own 231s
    are not heard - the transmitter is off and the game never turns it
    on (the model's choice; `docs/vv51.md` §13, 13).

## 7. The core

`quartus_sh -t boards/de10nano/core.tcl`, reports in
`build/mister/output_files/`.  Built twice: on a17d511
(`build/baseline-core.log`), and again on 26b39da
(`build/baseline-core2.log`, 2026-11-06, 20 minutes) after step 6
changed `rtl/chips/kr1818vg93.sv` (a state more in the write's
sequence).  The figures are the second build's, the first's in
brackets.  A third build (`build/baseline-core3.log`, 2026-11-06 15:39
to 16:05) after step 6 changed `rtl/chips/kr580vv51.sv` (the character
owed): 13 983 ALMs (33 %), the memory, RAM and DSP blocks as before;
every domain met; the 120 MHz domain's setup slack +0.633 ns, Fmax
129.87 MHz, hold +0.374 ns; the least setup slack of all +0.387 ns
(HDMI's PLL), the least hold +0.220 ns (the same).  A fourth
(`build/baseline-core4.log`, 2026-11-06 18:09 to 18:38) after the same
chip's second change (the character held in the shifter): 13 850 ALMs
(33 %), 17 760 registers, the memory, RAM and DSP blocks as before;
every domain met; the 120 MHz domain's setup slack +0.630 ns, Fmax
129.82 MHz, hold +0.364 ns; the least setup slack of all +0.304 ns
(HDMI's PLL), the least hold +0.208 ns (the same).  The `.rbf` in
`build/mister/output_files/` is this build's.

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
  It ended on 2026-11-06 at 21:32, 9.8 hours in all
  (`build/route-probe2/freerouting.log`): unrouted after each pass 592,
  431, 326, 264, 221, 194, 176, 156, 177, 162, 140, 140, no violation
  by Freerouting's count.  So with this placement and these rules the
  two outer layers did not take the board in 12 passes - **140 of 1258
  left** (the four-layer routing had 64 left after its twelfth), and
  the last five passes moved between 177 and 140, the twelfth gaining
  nothing.  More passes were not tried.
- Not done, and not to be done on this routing: DRC to no errors, Э3's
  board from this one.  What the board's stack is to be is the owner's
  to say, the probe having ended (open list, 5).

## Open after this walk

1. Omega's startup to its end (step 6) - done, pass.  A command typed
   at the prompt (`keys=FILE`, the МС 7004's codes by the key's place):
   tried on 2026-11-07 with `R SABOT2` at 24.2 s on the games diskette
   made from the owner's system (`build/games-omega`, the min corner to
   33 s) - **nothing is echoed**, and the `irq` log shows why: the key's
   bits reach D22's RxD, but the request `ЗАПР КЛ` never rises, where
   under the dec kit's diskette it rises 2 ms after the key.  D22's
   receiver stays idle because its clock is gone: D9's counter 0 holds
   1, not the ROM's 26 (seen inside the model, a bench print made for
   the look and taken out again).  The writer, found in the emulator and
   then seen on the board at 10.417 s of its boot from power-on
   (`build/games-omega/boot-d9`, the same print: the fetches at 005214,
   005222, 005226 and counter 0's reload going from 26 to 1): the
   installation check of EX.SYS, the electronic disk's handler on
   that diskette, which Omega's bootstrap runs at boot - `MOV #177530,R5;
   MOV #210,6(R5); MOV #1,(R5); CLR 2(R5)`, the module's ВВ55 at
   177530-177536 (ms0515-devel's `handlers/ex/EX.MAC`).  On Э3 as drawn
   D80 decodes `АД05`-`АД07` alone, so these are D9's registers too:
   counter 2 latched, counter 0 set to 1, counter 1 to 0 - and with N =
   1 in mode 3 the model's output is low 32 768 clocks of every 32 769, no baud
   clock for the keyboard.  The owner's machine had the wire from `АД03`
   to D80.5 (`docs/findings.md`, "The green wire"), which keeps these
   addresses off the board's own chips: there the same boot left the
   keyboard alive, as it must have.  The emulator decodes the timer at
   177520-526 only, and its ВВ51 has no clock to lose, so it cannot show
   this.  Not the model's error - the board as drawn, with a system
   diskette made for the machine with the module.  Run again with EX.SYS
   taken off the diskette (`build/games-omega2`, begun 2026-11-07 02:05,
   three corners, `R SABOT2` at 24.2 s, to 52 s): **the keyboard lives**
   - at min the nine keys are echoed as typed and the Enter taken, the
   cursor on the next line at 26.5 s (`build/games-omega2/min26.png`,
   the prompt at 24 s beside it), and the game's title at 28.5 s on all
   three corners - a program loaded from Omega's disk, and item 9 has
   its second handler (the same 20 s under it).  Whether `SET SL ON`
   took effect the screen cannot say: `SET TT QUIET` before it stops
   the command file's echo, and the prompt shows `.SET TT QUIET` alone;
   a look into a snapshot is left.
2. The core's drive against the bench's (step 6): found and fixed -
   the controller's model ended a sector's write a byte early and the
   bench's drive then lost the write; the core's drive was right.  The
   core is built again (step 7), the boot runs made again on the fixed
   board: 17 of 17 pass, the two drives' images the same after Omega's
   eight writes.  Closed.
3. The cursor's 8 samples in the self-test's frames at typ (step 6) -
   looked into on 2026-11-07 (`build/cursor`: the run without a keyboard
   made again to 5.6 s, a frame's nets tick by tick with `edges=`, half
   a second with every frame kept).  Not the blink's phase.  The words
   of «Клавиатура не готова» flash (attribute F2), and so do the two
   lines of the cursor after it (B8); the six lines above the cursor
   are plain (38).  In the half of the flash where the message stands
   inverted, the plain word after it shows its ink for 3 master ticks
   (25 ns) before its paper: the attribute registers change at tick
   660813570, the output goes from the message's red to black at 572
   and to grey at 575.  The colours go from the registers to the
   inputs of the multiplexers D63, D69; their selection ВК8 H comes from
   D55's БЛНК through D31 (with ТМ H and РАЗР L) and D39's XOR with the
   point ДПСЛ H - two gates later.  ТС15 H rises
   at 574, inside it, and the bench takes its sample there: one black
   sample at the word's start on six lines, and one after the cursor's
   bar on its two - the 8; on the earlier screens the 2 after the
   cursor's bar alone.  The emulator's picture has no such stretch, and
   the compare lets a flashing word be in either phase, so only the
   plain words next to one are counted.  On a monitor it is 25 ns of
   the next word's ink at the edge of an inverted field - a fifth of a
   320 x 200 point; at the corners the compare's one sample of slack
   takes it.  It is the board's, at the typical delays of К555ЛА4 and
   К555ЛП5, of a kind with the attributes coming 17 ns before the
   points (`docs/timing.md`, "Corners").  Nothing changed.  Closed.
   Seen on the way: the flash turns every 8 frames, 16 frames a cycle
   (`build/cursor/f`: frames 336, 344, 352, 360) - ТМ H is D59's count
   of the frame sync by 16, 3.76 Hz at 60.1 frames a second.  The kept
   frames, every 30th, showed it as 120 frames a phase.  The emulator
   turns its flash every 30 frames (its `docs/hardware/video.md`); the
   compare takes either phase, so no run here depends on it.
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
   `tools/pcb.py` has to hand the inner layers over as planes.  The
   probe of that (step 8) left 140 of 1258 connections on the two outer
   layers after 12 passes, so the stack is the owner's to choose: two
   more signal layers, other rules (track and clearance), another
   placement, or more passes.  After that: DRC, the silkscreen's
   warnings, Э3's board from it.
6. Э3's board, stage 6b - paused.
7. The cell 000:000 at max from an unseeded power-up (step 6, the games
   diskette's boot): 900 reads of it not as written, where ОСА's run
   had 2.  Looked into: the bench's watch took the strobes standing low
   at its first tick for a write no chip made; all the reads are the
   processor's in the first 0.207 s, before the cell's first write.
   The watch fixed, with a test.  Closed.
8. Did the keys click in BIRDS and Saboteur 2 on the machine under
   Omega?  Asked of the owner (`docs/vv51.md` §13, 13): it tells what
   КР580ВВ51А does with a character written while TxEN is off - the
   model keeps it in the shift register, and the games' 231 ("no key
   click") then never reaches the keyboard.  **The owner's answer
   (2026-11-07): in Saboteur 2 the keys did not click** - the game's
   command turned the click off, as the emulator's keyboard notes have
   it (`docs/hardware/keyboard.md`, 231); BIRDS he never had.  So on
   the machine the 231 reached the keyboard, where the model holds it
   for a TxEN that the game (its command 064 leaves TxEN off) never
   gives: the model's choice is contradicted, unless the click was
   already off at Omega's prompt - asked back.  If it clicked at the
   prompt and not in the game, the real chip sends a character written
   with TxEN off, and `rtl/chips/kr580vv51*.sv` has to follow (§7's
   choice turned over) - a model change, the games' writes and the
   owner's machine its evidence, to be made with a test at the corners.
9. Saboteur 2's loading on the board: 20 s, a block a turn of the
   diskette (step 6, the games), where the emulator takes 7 to 8 s from Return to the menu with
   a controller that knows no turning.  Both handlers begin a request
   with Read Address, so the board's 240 ms a block is what the code
   and a diskette at 300 rpm give - if the controller's model is right
   about a Read Sector given just after its ID has passed.  The run of
   the same game from Omega's own system diskette (`build/games-omega`,
   2026-11-06 23:09) typed into a dead keyboard (item 1); its second
   try, without EX.SYS (`build/games-omega2`, 2026-11-07, all three
   corners to their end at 52 s; max with 355 102 video requests lost
   as in the boots and its fetches 6 % behind typ's - item 10), shows
   **the same 20 s under Omega's
   handler**: Return at 24.68 s, the title's picture at 28.5 s on typ,
   min and max alike (`build/games-omega2/min42.png`; the program's
   own load 3.8 s, the dec run's 1.8), then the diskette read a block
   every 240 ms from 29.2 s to 49.5 s - 20.3 s against the dec run's
   20.4 (16.47 to 36.83 s) - then 2.4 s of disk work without the pauses,
   and the title cleared at 51.8 s, where the run ended (the dec run's
   menu came 2.6 s after its clearing).  So the block a turn is not the
   handler's - both give it - and the question stays with the
   controller's model: a Read Sector given just after its ID passed,
   and whether a real diskette took as long under Omega's handler (the
   dec kit is our own build from DEC's sources, the owner reminds - no
   such system ran on a machine, so only Omega's 20 s can be asked
   about).  (A first reading of this run, written and withdrawn the
   same morning, took the title's picture for the whole game loaded.)
10. The processor's pace at max.  At a system's prompt the board at max
   makes 4.4 % fewer fetches a second than at typ, and at min 0.65 %
   more (the games diskette's boot, 13.5 to 14.5 s: max 170 645, typ
   178 451, min 179 609 a half second); the games' stages at max are 4
   to 5 % of their fetches behind, BIRDS' title a second later.  Through the ROM's start the
   three make the same fetches (77 135 in the first 0.3 s, `build/pace`)
   - the difference is in programs that run from the dynamic RAM.
   Measured at the prompt (`build/pace/p<corner>.log`, 20 ms of every
   fetch from the snapshot at 14 s): 2.803 us a fetch at typ, 2.929 at
   max, 2.784 at min; the same instruction takes longer at max by a
   part of a microsecond - 142606 always 4 us at typ and min, 4 or 5 at
   max; 110750 1.57 us on the average at typ, 2.00 at max.  The step is
   of the size of T-11's READY slip (a microcycle, 0.4 us;
   `docs/t11-model.md`), READY being `ЗАДЕРЖ L` from D98 - the
   processor waiting for the video's read of the memory.  Not shown:
   the bench prints a fetch's time to the microsecond and has no count
   of the slips.  Queued for the owner's word: a count of them in the
   bench, and with it what at max makes them more.

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
