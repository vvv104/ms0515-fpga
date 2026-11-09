# The project's map

What the project is made of, what is made from what, by which tool, and
how the whole chain is walked again from the start.  Read this first -
after a pause, after a context reset, before reporting where anything
stands.  Where things stand is not written here: it is `docs/plan.md`
(the stages) and `docs/baseline.md` (what was last run, on which commit,
with what result).  A status is only ever said with its reference - a
commit, a test, a log, a section of a document; what has none is "not
confirmed", never "done".

## What comes out

One source, two things made from it, and one check of both:

- **The source**: НС4, the system board of the MS 0515, read off its
  schematic 3.858.420 Э3 and parts list ПЭ3 into `netlist/` - every part,
  every pin, every net - and its chips as modules of their own logic in
  `rtl/chips/` from their datasheets.
- **The FPGA core**: the board as a MiSTer core for the DE10-Nano
  (`docs/plan.md` stage 7) - the board generated from `netlist/`, the
  drive, keyboard and video around it.
- **The printed board**: НС4 in copper again, laid out anew in KiCad
  (stage 8) - the same `netlist/`, as parts on a board.
- **The check**: the board simulated with Verilator against the
  emulator (`../ms0515`, the MS 0515 emulator, an independent model from
  other sources), fetch for fetch and point for point (stage 6).

Two versions of the board (owner, 2026-11-05): Э3 as drawn (`ns4`) and
the owner's machine as it worked (`vvv104` = Э3 + `netlist/vvv104.csv`,
D70.15 on GND).  Everything below serves both; where they differ is
`netlist/vvv104.csv` alone.

## The sources (written by hand)

| Where | What | Read off |
|---|---|---|
| `netlist/parts.csv` | the parts: ref, type, value, pins, power pins | ПЭ3, Э3 sheet 1's power table |
| `netlist/pinouts.csv` | every chip type's pins: number, name, direction | the chips' datasheets |
| `netlist/nets.csv` | every net: its pins, the sheet and zone read from | Э3 sheets 1-5 |
| `netlist/deviations.csv` | places where the drawing cannot be right, each argued | `docs/findings.md` |
| `netlist/vvv104.csv` | the owner's board's change to Э3 (a pin moved), its evidence | his board's photos |
| `netlist/analog.csv` | each analog part's digital stand-in | `docs/analog.md` |
| `netlist/fitted.csv`, `fitted-others.csv`, `fitted-compare.md`, `fitted-asked.md`, `fitted-passives.md`, `fitted-1991.csv` | what is fitted on the owner's board and on Frolov's and Radon17's, read off the photographs: the КР1533 substitutes for К555 by position and production run, dates; the three compared, and what it says for the 640 x 200 edge; what the photos did not tell; `fitted-1991.csv` is read by `tools/board.py` for every generated board | `netlist/variants.md` |
| `netlist/delays.csv`, `delays-max.csv` | each chip path's typical / maximum delay, with its page | datasheets, `docs/timing.md` |
| `netlist/firmware.csv` | which ROM/PROM image sits in which chip | `firmware/` |
| `netlist/placement.csv` | where each part stands on the 1991 board | the assembly drawing СБ |
| `netlist/variants.md` | the board's build variants (jumpers, sockets) | Э3, ТО |
| `netlist/packages.csv` | each non-chip part type's footprint and equivalent | for stage 8 |
| `netlist/pcb_parts.csv`, `pcb_nets.csv`, `pcb.csv` | what the new printed board adds (jumper E9) | owner's decision, stage 8d |
| `netlist/ms7004/*.csv` | the МС 7004 keyboard: parts, pins, nets, matrix, PC keys | its own documents (`docs/ms7004.md`) |
| `netlist/open.md` | reads not settled yet | |
| `rtl/chips/*.sv` | one module per chip type, pin for pin | datasheets; T-11 Engineering Specification |
| `rtl/parts/*.sv` | the core's parts around the board: drive, keyboard translator, video out | `rtl/parts/README.md` |
| `rtl/ms0515.sv`, `boards/de10nano/*.sv`, `*.sdc`, `*.tcl` | the core: the machine, its MiSTer `emu`, clocks, timing, build scripts | stage 7d |
| `boards/de10nano/template` | MiSTer's framework, a git submodule (not ours, not copied) | Template_MiSTer |
| `firmware/*.rom`, `*.r12` | the machine's ROMs and the video PROMs, as read from real chips | `firmware/README.md` |
| `firmware/tests/*.mac` | test programs (MACRO-11) for parts of the board the ROM leaves alone | |
| `sim/` | the test benches (C++), the bench's drive, keyboard stub, supply | `docs/bench.md` |
| `docs/` | the analyses, findings, plan, sources | |

## The derived files (made by a command, kept in the repository)

Never edited by hand.  Each generator has `check`: it makes the file
again in memory and fails if the committed one differs.

| File | Made by | From | Check |
|---|---|---|---|
| `rtl/board/ns4.sv`, `ns4_inputs.inc`, `ns4_machine.sv`, `ms7004.sv`, `ms7004_machine.sv`, `ms7004_inputs.inc` | `python tools/board.py` | `netlist/*.csv`, `netlist/ms7004/*.csv`, the chip modules' ports, `delays.csv` | `python tools/board.py check` |
| `rtl/parts/ms7004_keys_pkg.sv` | `python tools/ms7004_keys.py` | `netlist/ms7004/pc_keys.csv`, `matrix.csv` | `... check` |
| `firmware/tests/*.rom` | `python tools/test_roms.py` | `firmware/tests/*.mac`, assembled by DEC's MACRO/LINK run in the emulator's `ms0515-run` | `... check` |
| `kicad/ns4*.kicad_sch` | `python tools/kicad.py` | `netlist/` | `... check`; with kicad-cli a tool test reads the nets back |
| `docs/pcb/bom.md` | `python tools/bom.py` | `parts.csv`, `pcb_parts.csv`, `packages.csv`, `docs/chips.md`, `firmware.csv` | `... check` |
| `kicad/ns4-vvv104.kicad_pcb`, `kicad/ns4.kicad_pcb` (placement) | `<KiCad>/bin/python tools/pcb.py vvv104`, `... pcb.py` | `netlist/` (+ `vvv104.csv`) + `placement.csv` + footprints; the parts stand where Э3's nets put them on both | `... pcb.py check [vvv104]`: every pad's net against the netlist |
| the same files (routing) | `tools/route.py [vvv104] start / run / finish` (Freerouting), state in `build/route[-vvv104]` | the placed board | not repeatable bit for bit: checked by `pcb.py check` and KiCad's DRC |
| the second board | `tools/pcb.py derive TO FROM` + `route.py ... start --from` | the first board as routed; only the nets its pins left are routed again | `pcb.py check` |

## The comparison files (made by a command, not kept)

How `nets.csv` was checked against the copper and against Mick's
independent reading.  They are in `.gitignore`: made again from
`$MS0515_DATA` whenever wanted, and what they say is kept in words in
`docs/findings.md`.  They go nowhere further down the chain: a
difference found there is settled by hand in `nets.csv` (or
`deviations.csv`) with its reasoning in the findings.

| File | Made by | From | Use |
|---|---|---|---|
| `netlist/copper.json` | `python tools/copper.py` | Mick's / Radon17's layer traces (`$MS0515_DATA`) | for comparison only |
| `netlist/registration.json`, `verify.json`, `verify-*.png` | `tools/register.py`, `tools/verify_traces.py` | the bare-board scans | for comparison only |
| `netlist/footprints.json` | `tools/footprints.py` | `copper.json` | for comparison only |
| `netlist/mick.csv` | `tools/mick_pdf.py` | Mick's PDF of Э3 | for comparison only |
| `netlist/compare.txt`, `compare-mick.txt` | `tools/compare.py`, `tools/compare_mick.py` | `nets.csv` against the copper / Mick | the read-out's cross-check |

Not in the repository, made in `build/`: the Verilator models and test
executables, the corner boards (`build/min`, `build/max`), the Quartus
project and the core's `.rbf` (`build/mister`), the routing's state
(`build/route*`), the runs' logs and frames.

## The tools

| Tool | What it is | What it does here | Stage |
|---|---|---|---|
| Python 3 | | every generator and check in `tools/`; their tests `python -m unittest discover -s tools/tests` | all |
| Verilator 5.044 (Conan) | turns SystemVerilog into C++ | the board and chips become C++ models; `sim/` tests them against the emulator | 3-6 |
| Conan, CMake, Ninja, MSVC | the C++ build | `conan build . --build=missing -s compiler.cppstd=20` builds and runs every test (ctest) | 3-6 |
| The emulator `../ms0515` | the MS 0515 emulator (C), our other project | the reference model linked into the tests; its `ms0515-run` assembles the test ROMs | 6 |
| Quartus Prime Lite 17.0.2 | Intel's FPGA compiler | `quartus_sh -t boards/de10nano/core.tcl`: synthesis, fit, timing at 120 MHz, the `.rbf` | 7 |
| KiCad 10 | printed-board editor | its Python (`pcbnew`) runs `tools/pcb.py`, `route.py`; `kicad-cli` reads the schematic back, runs DRC, writes Gerbers | 8 |
| Freerouting 2.5.0 (Java) | autorouter | `tools/route.py` hands it the placed board (DSN) and takes the routing back (SES) | 8d |

Where they are found (no machine paths in the repository): the emulator
`$MS0515_EMULATOR_DIR` else `../ms0515`; `ms0515-run` `$MS0515_RUN` else
`../ms0515/package/`; the software collection `$MS0515_SOFTWARE` else
`../ms0515-software`; scans, photos, Freerouting's jar `$MS0515_DATA`
else `../ms0515_data` (`$FREEROUTING_JAR`); kicad-cli `$KICAD_CLI` or the
PATH; KiCad's footprints `$KICAD10_FOOTPRINT_DIR`.  The Windows build
needs Visual Studio's installer folder on the PATH (vswhere) before
`conan build`.

A gap to know: the emulator is not pinned to a commit - the tests take
`../ms0515` as it stands.  `docs/baseline.md` writes down the emulator's
commit of each run.

## The chain, walked from the start

Each step only on the steps before it being green.  What each step
proves is said with it.

1. **The netlist is consistent.**  `python tools/netlist.py check` and
   `check vvv104`: every pin on one net, every part's pins known, pin
   names as drawn against the datasheet's.  `python -m unittest discover
   -s tools/tests` (with `$KICAD_CLI` set, or one test is skipped).
2. **The netlist is the copper** (needs `$MS0515_DATA`): `copper.py`,
   `compare.py`, `mick_pdf.py`, `compare_mick.py`, `verify_traces.py` -
   the read-out against the board's traces and Mick's independent
   reading; the counts they print are those `docs/findings.md` records,
   and every difference has its line there.
3. **The generated files are current**: `board.py check`,
   `ms7004_keys.py check`, `test_roms.py check`, `kicad.py check`,
   `bom.py check`.
4. **The chips and the board, typical delays**: `conan build .` - every
   chip against its function table, the processor in lockstep with the
   emulator's, the board from power-on, the test ROMs' pictures, the
   drive, the keyboard, the core's machine.
5. **The board at its delay corners**: `conan build . -o "&:corner=min"`
   and `max` (build/min, build/max); check `MS0515_BOARD_SV` in
   `build/<corner>/CMakeCache.txt` names the corner's board.
6. **The real software on the board**: `run_board` (`docs/bench.md`),
   from `build/Release/sim`, `build/min/sim`, `build/max/sim`, the output
   folder made beforehand (it is not made by the run):
   `run_board 20000 FOLDER 30 disk=IMAGE keyboard [seed=N] [rom=ROM-A]`
   - Omega (`test_vvv_system.dsk` of the emulator's
   `src/lib/tests/disks/originals`) and ОСА (`test_osa_games.dsk`,
   `rom=firmware/ms0515-roma.rom`).  **`keyboard` is needed**: without the
   stub ROM-B stops at «Клавиатура не готова» and never reads the disk.
   What is checked, and what is not: up to ROM-B's keyboard request the
   fetches are the emulator's and the frames its screen; from there the
   emulator (given the ROM alone - no keyboard, no disk) goes another
   way, the log says `fetch N: the board at ..., the emulator at ...`,
   and the run is judged by its own lines - the frames seen (the
   system's prompt), `RAM: ... 0 differ` (every DRAM read as written),
   the video requests lost.  Check a frame after the first minutes
   before leaving a run for hours.
7. **The core**: `quartus_sh -t boards/de10nano/core.tcl` -
   `build/mister/output_files/ms0515.fit.summary` (resources),
   `ms0515.sta.rpt` (Setup Summary: the 120 MHz domain's slack).
8. **The printed board**, Э3's first, eight layers: `<KiCad>/bin/python
   tools/pcb.py` (placement), `tools/route.py start`, `run 8 3`,
   `finish` (routing from the empty board, a day and more; `run` again
   goes on after a stop; progress in `build/route/progress.txt`),
   `pcb.py check` (pads against the netlist), `kicad-cli pcb drc`
   (clearances, shorts).  Then the owner's from it: `pcb.py derive
   vvv104`, `route.py vvv104 start --from ns4`, `run`, `finish`.

Steps 1-5 take about half an hour; step 6 about two hours (side by
side); step 7 about half an hour; step 8 a day.

## When to run what

A change to                                  | runs
---|---
`netlist/`, `rtl/chips/`, `tools/board.py`, `firmware/` | 1, 3, 4, **5** (all three corners), and 6 if the board's behaviour can change
a test of the board                           | 4 and 5: a new test is run at every corner on its first day
`rtl/parts/`, `rtl/ms0515.sv`, `boards/`      | 4, 7
the PCB tools, `placement.csv`, `pcb*.csv`    | 8 (`pcb.py check`, DRC)

## Where things stand, and how it is said

- `docs/plan.md` - the stages, what each is for, its state.
- `docs/baseline.md` - the last full walk of the chain: the commit (and
  the emulator's), every step's command and result.
- `docs/findings.md` - every disagreement between sources, and its
  settling or its being open.
- `docs/evidence/` - the pictures and traces behind a finding, a folder
  a case.

Rule: a state is reported only from these files, with its reference.
Who was asked what, and who answered, likewise: only as a document
records it.
