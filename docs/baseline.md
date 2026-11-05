# Baseline: the chain walked from the start

The last full walk of `docs/map.md`'s chain, step by step: the command,
the result, where the result is kept.  Nothing counts as passing that is
not in this file with its run.  The board run is the owner's (`vvv104`,
the model's default: D70.15 on GND); Э3's board is paused at stage 6b
(`docs/plan.md`).

**Begun 2026-11-05** on commit a17d511 (trees: `rtl` 303e8f4, `sim`
62cd21d, `firmware` 1a957b3); the emulator `../ms0515` at 4325511.
Steps marked *running* or *to do* have no result yet.

| Step | State |
|---|---|
| 1. The netlist is consistent | pass, after a fix (below) |
| 2. The netlist is the copper | reproduces; 4 of Mick's differences not written up |
| 3. The generated files are current | pass |
| 4. Chips and board, typical delays | pass, 72 of 72 |
| 5. The corners | **fail**: min 2 tests, max 2 tests (below) |
| 6. The real software on the board | running |
| 7. The core | pass: timing met, +0.647 ns at 120 MHz |
| 8. The printed board | to do: placed, not routed |

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
- **Fixed**: `pinouts.csv` has a column `drawn` - Э3's label where it
  differs from the datasheet's name - and the checker takes either; two
  tests now run the checker on `netlist/` itself, as Э3 and as `vvv104`.
- **Result**: 0 errors both ways; 315 parts, 534 nets, 2129 pins on
  nets, 47 pinouts.  One warning, known: net `НАЧ H` has one pin
  (D97.12).  Tool tests: 111 of 111, none skipped.

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
- **Not written up**: the findings settle D70.5, `ИМП ЧТ Н`, D62/D67,
  D84.12/13, D7.14, D81.9, `ТС45 L`, `СГИ L`/`КГИ L` and say "so far".
  Four places of the 20 lines are named nowhere in `docs/`: `ЗАДЕРЖ L`
  (D83.26 and D98.9 on two unnamed nets of Mick's), `ЗАП РЕГ H` (D79.3
  on Mick's `ЗАПРЕГ L`), `РВК1 H` (E3.1 on Mick's `АД05 H`), `ЗЦ H`
  (D81.10 on Mick's `ИЦ H`).  **Open: each read on Э3 at full
  resolution and written into the findings.**

## 3. The generated files are current

`board.py check`, `ms7004_keys.py check`, `test_roms.py check`,
`kicad.py check`, `bom.py check`: all exit 0 - every generated file in
the repository is what its sources make.  `board.py` warns of four nets
nothing drives: `~D10.14`, `~D70.14`, `~D70.13`, `~D89.18`.

## 4. Chips and board, typical delays

`conan build . --build=missing -s compiler.cppstd=20`: **72 of 72 tests
pass** (1085 s); log `build/baseline-typ.log`.  The two tests of Э3's
picture (`video: Э3's board draws ...`) are marked may_fail and fail, as
stage 6b says.

## 5. The corners

`conan build . ... -o "&:corner=min"` / `max`; logs
`build/baseline-min.log`, `build/baseline-max.log`.  70 of 72 at each.

- **min, `test_parts`: the picture is not the emulator's.**  `video: 320
  x 200 with every attribute` differs in 4222 samples (first at 31, 0),
  `video: 640 x 200 in the border's colour` in 100 (first at 639, 1).
  Э3's board gives the same two numbers, so D70's change has no part in
  it.  No change made it so: on the commit that brought these tests
  (421012f, 2026-11-03) they fail at min with the same numbers - they
  had only ever been run at typ.  (`docs/plan.md` 7b's "at the min
  corner the screen is clean" was of ROM-B's own screens, 2026-11-01,
  before these tests were.)  The min corner is half the typical delay
  where a datasheet prints no minimum - an assumption (`docs/timing.md`).
  **Open: the cause is not known.**
- **min and max, `test_machine`**: the core's machine
  (`rtl/board/ns4_machine.sv`, typical delays - the corner generator
  makes `ns4.sv` only) is compared with the bench's board at the corner.
  A test that holds at typ only; **open: say so in the test or build the
  machine at the corner too.**
- **max, `test_parts`, sound**: `quiet_lows == 0` fails with 2 - the
  node low for 4 ticks and more while the way is shut; the test's 4
  ticks stand for D93's delay at typ.  **Open: measure the pulse at max
  before calling it the test's threshold.**

Neither `test_machine` (2026-11-01..03) nor the picture and sound tests
(2026-11-03) had been run at a corner before: the corners were last run
on 2026-11-01.  Hence the rule in `CLAUDE.md`: every corner when the
board can change, a new test at every corner on its first day.

## 6. The real software on the board

`run_board 20000 <folder> 30 disk=... seed=N`, side by side, into
`build/baseline/<run>/run.log`:

| Run | Disk | Corner | Seeds |
|---|---|---|---|
| Omega | `test_vvv_system.dsk` | typ | 0, 1, 2, 3 |
| Omega | `test_vvv_system.dsk` | max | 1, 2, 3 |
| ОСА | `test_osa_games.dsk`, ROM-A | typ, max | 0 |

*Running.*

## 7. The core

`quartus_sh -t boards/de10nano/core.tcl`; log `build/baseline-core.log`,
reports in `build/mister/output_files/`.

- Resources (`ms0515.fit.summary`): 13 963 of 41 910 ALMs (33 %),
  1 643 857 of 5 662 720 block memory bits (29 %), 214 of 553 RAM
  blocks, 33 of 112 DSP blocks (29 %).
- Timing (`ms0515.sta.rpt`, the template's single model): every domain
  met.  The 120 MHz domain (`emu|pll ... general[0]`): setup slack
  +0.647 ns, Fmax 130.11 MHz, hold +0.350 ns.  The least setup slack of
  all: +0.469 ns (HDMI's PLL, the template's own).
- Not done: the post-fit netlist on the bench (plan 7d).

## 8. The printed board

To do.  `kicad/ns4.kicad_pcb` in the working copy is placed with E9 and
not routed (not committed); the routing begins from the empty board
(`docs/plan.md` 8d).

## Open after this walk

0. Four of Mick's differences not written up (step 2) - the earliest
   in the chain.
1. The min corner's picture (step 5).
2. `test_machine` at the corners; the sound test's threshold at max.
3. Э3's board, stage 6b - paused.
