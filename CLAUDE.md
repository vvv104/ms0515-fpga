# MS0515 FPGA Project

**Read this first after compacting!**

## What this is
The Elektronika MS 0515 rebuilt in an FPGA **down to its circuits**: the
system module НС4 (`3.858.420 Э3`) as a netlist of its chips, each chip a
module with its real pins, wired exactly as the schematic and the board
wire them.  Not an emulator in HDL - a copy of the board.

- **The board is the netlist.**  `netlist/` holds НС4 read out of the Э3
  sheets and checked against the bare-board scans (rev. 3А): every part,
  every pin, every net, each with the sheet and zone it was read from.
  The top-level RTL of the board is generated from it, never written by
  hand.
- **A small-scale chip is its logic.**  К555/К1533/К531 parts (ЛА3, ТМ2,
  ИР23, ИВ3, ИД7, КП2, ИЕ19, ...) are modules that do what the datasheet's
  function table says, pin for pin, including their asynchronous inputs.
- **A large-scale chip is its pins.**  КР1807ВМ1 (T-11), КР1818ВГ93,
  КР580ВИ53, КР580ВВ51, КР580ВВ55, the RAMs and ROMs are modelled at their
  pins with their bus cycles and timings as their documentation gives them
  (the T-11 Engineering Specification and User's Guide for the processor).
  Their insides are original code; nothing is copied from other models.
- **Analog stays outside.**  The op-amps, RC one-shots, the crystal and the
  video DAC are replaced by their digital effect (a one-shot becomes a
  counter of the same duration), and each such replacement is written down
  in `docs/analog.md` with the parts it stands for.
- **Everything runs on one clock.**  The FPGA has no ripple and no gate
  delay: the board's asynchronous logic is evaluated synchronously at a
  master clock many times the board's, so every ripple counter, gated clock
  and edge keeps its order.  How each such place is mapped is documented
  next to the module.

## Layout
```
netlist/   — НС4 read out of the scans: parts, pins, nets, sources
rtl/
  chips/   — one module per chip type (К555ЛА3, КР1807ВМ1, ...)
  board/   — the board generated from netlist/ (never edited by hand)
  top/     — per-FPGA-board tops: clocks, HDMI, SD card, keyboard line
sim/       — Verilator test benches (C++), the emulator as reference model
boards/    — FPGA-board constraints and build scripts (MiSTer / DE10-Nano: docs/plan.md stage 7)
kicad/     — the KiCad schematic generated from netlist/ (tools/kicad.py; never edited by hand)
tools/     — the netlist reader, the RTL generator, scan helpers (Python)
docs/      — sources, plan, analog replacements, findings
```

## Build
```
conan create recipes/verilator --version 5.044    # once: ConanCenter has no Verilator
conan build . --build=missing -s compiler.cppstd=20
```
builds every test bench (Verilator, CMake and Ninja come from Conan) and
runs them with ctest.  `sim/CMakeLists.txt`: `ms0515_chip_test(<chip>)`
verilates `rtl/chips/<chip>.sv` into its own library `V<chip>` and links
`sim/chips/test_<chip>.cpp` against it.  The generated models and the
Verilator runtime are a tool's output: they build in their own libraries
without our warning flags, their headers reach our code as system headers;
our code (`ms0515_strict`) builds with `/W4 /WX` or `-Wall -Wextra -Werror`.
A chip module is its pins: a port `pN` for every signal pin N, power pins
left out.  `python -m unittest discover -s tools/tests` tests the tools.
`python tools/board.py` regenerates `rtl/board/ns4.sv` after a change to
`netlist/` or to a chip module's ports (a tool test fails while the
committed file is stale); `sim/board/test_board.cpp` runs it.
`python tools/kicad.py` regenerates the KiCad schematic `kicad/` the same
way; with KiCad's command line (`$KICAD_CLI` or `kicad-cli` on the PATH) a
tool test reads the nets back out of it and compares them with the netlist.

## Sources
`docs/sources.md` lists every document, scan and forum thread the board is
read from.  The scans themselves are **not** in this repository: they are
found through the environment variable `MS0515_DATA`, falling back to
`../ms0515_data` beside the repository.  Every netlist entry and every
behaviour a module has names its source; a line nothing can source is not
written.  Where the schematic, the board and the software disagree, the
disagreement is recorded in `docs/findings.md` and the user decides.

## Reference model
The emulator (https://github.com/vvv104/ms0515, found through
`MS0515_EMULATOR_DIR`, falling back to `../ms0515`) is linked into the test
benches: its `src/core` runs beside the RTL and the two are compared after
every instruction (registers, PSW, memory writes, cycle count).  The
emulator never depends on this repository.  Where the board and the
emulator disagree, the board is checked against its sources first; a fix
the emulator needs goes to the emulator's repository, by its rules.

## Key rules
- All code, comments, and documentation must be in **English only**
  (chip and signal names as printed on the schematic stay as printed).
- Write original code based on the documentation; do not copy from other
  models (cpu11/t11, MAME, ms0515btl, MiSTer cores) - read them as
  references only.
- **Never commit or push** without explicit user permission.  Commits to a
  feature branch are allowed without asking; commits to `main` and any push
  still require permission.
- **CI gates every merge**, once CI exists: open the PR → CI green on the
  PR → merge → CI green on `main`.  Fixes go through the feature branch.
- **Test-driven development**: every chip module gets its function-table
  test before its implementation; every board block gets its test against
  the reference model before it is wired.  Run tests at each stage.
- **Revert failed attempts**: roll back unsuccessful approaches; no dead
  code.
- **Zero warnings**: Verilator `--lint-only -Wall` and the synthesis tools
  report nothing; the C++ test benches build without warnings under
  `/W4 /WX` and `-Wall -Wextra -Werror`.
- **Never suppress warnings**: no `verilator lint_off`, no
  `/* synthesis translate_off */` around real logic, no `#pragma warning`.
  Rewrite the code instead.
- **The language standards are declared once**: SystemVerilog (the
  synthesizable subset: `logic`, `enum`, `struct`, `always_ff`,
  `always_comb`, no classes) in the build files; C++20 for the test
  benches in the top `CMakeLists.txt`.
- **No vendored third-party sources**: C++ dependencies through Conan;
  no third-party HDL in the tree.
- **No machine-specific paths in tracked files**: environment variable +
  repository-relative fallback.  Before every commit
  `git grep -i "users[.]voron"` must return nothing.
- **No session links in the history**: commit messages and pull request
  bodies carry the `Co-Authored-By:` trailer and the "Generated with Claude
  Code" credit, and nothing else.
- **Generated files are generated**: `rtl/board/` comes from `netlist/` by
  `tools/`; fix the netlist or the generator, never the output.
- Functions in tools and test benches stay under 100 lines.
