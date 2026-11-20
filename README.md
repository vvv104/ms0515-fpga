# ms0515-fpga

The Elektronika MS 0515 rebuilt in an FPGA down to its circuits: the
system module НС4 (`3.858.420 Э3`) as a netlist of its chips, wired as the
schematic and the board wire them, tested against the emulator
(https://github.com/vvv104/ms0515).

- `docs/sources.md` - the documents, scans and threads it is read from
- `docs/plan.md` - the stages
- `docs/findings.md` - where the sources disagree
- `CLAUDE.md` - the rules of the repository

## Making the core and the fab's files

Neither the FPGA core nor the printed boards' Gerbers are kept in the
repository: both are made from it, each with one command.

**The MiSTer core** (`ms0515.rbf`, for a DE10-Nano):

1. Quartus Prime Lite 17.0.2, the version MiSTer's template is built
   with.
2. A clone with its submodule (`boards/de10nano/template` is MiSTer's
   Template_MiSTer): `git clone --recursive
   https://github.com/vvv104/ms0515-fpga`, or `git submodule update
   --init` in a clone made without it.
3. From the repository's root: `quartus_sh -t boards/de10nano/core.tcl`
   (`quartus_sh` is in Quartus's `bin64`), about 20 minutes.
4. The core is `build/mister/output_files/ms0515.rbf`; it goes into
   `_Computer/` on MiSTer's SD.  ROM-B is built in; ROM-A
   (`firmware/ms0515-roma.rom`) is loaded from the core's menu.  The
   build is good when `ms0515.sta.rpt` there shows a positive setup
   slack; what to check on the board first: `docs/plan.md` 7e.

**The Gerbers** of the two boards (KiCad 10 and Python 3):

1. KiCad's command line as `$KICAD_CLI`, or `kicad-cli` on the PATH.
2. `python tools/fab.py` for НС4, `python tools/fab.py
   kicad/ext.kicad_pcb` for the RAM-disk module.
3. In `build/fab/ns4/` and `build/fab/ext/`: `<board>-gerbers.zip` (the
   copper, masks, silkscreens, outline and drill files - what a fab
   takes) and `assembly-top.pdf`.  What to order and what to check first:
   `docs/pcb/fab.md` (НС4); the parts: `docs/pcb/bom.md`.  Neither board
   has been built yet, and the module's DRC still has 33 courtyard errors
   from the photos' placement, to be checked against the photos before
   ordering (`docs/baseline.md`, "Open after this walk").
