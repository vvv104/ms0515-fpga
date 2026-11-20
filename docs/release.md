# The ready files

The files attached to a release of this repository
(https://github.com/vvv104/ms0515-fpga/releases), made from the commit
its tag names, so they can be used without Quartus or KiCad.  How to
make them again and publish a release is at the end.  Nothing here has
been on real hardware yet: the core met timing and runs the systems on
the model, and the boards pass their checks (`docs/baseline.md`).

| File | What it is |
|---|---|
| `ms0515.rbf` | the MS 0515 as a MiSTer core for the DE10-Nano |
| `ns4-gerbers.zip` | НС4, the system board: the fab's files, 8 layers |
| `ns4-assembly-top.pdf` | НС4's assembly drawing, parts' side |
| `ext-gerbers.zip` | the RAM-disk module (memory expansion): the fab's files, 4 layers |
| `ext-assembly-top.pdf` | the module's assembly drawing, parts' side |

## The core: `ms0515.rbf`

1. A DE10-Nano with MiSTer on its SD card, a USB keyboard, HDMI to a
   monitor that takes 72 Hz (or `vsync_adjust` in MiSTer's ini for one
   that does not).
2. Copy `ms0515.rbf` into the SD card's `_Computer/` folder and choose
   MS0515 from MiSTer's menu.
3. Power-on with no diskette shows the ROM's self-test, «Клавиатура
   готова», then «НГМД не готов».
4. In the core's menu (F12):
   - *Drive 0* - an MS 0515 diskette image (`.dsk`; the emulator's
     repository https://github.com/vvv104/ms0515 has some), and
     *Disk layout* to match it (Omega, vvv, FDZ);
   - *Load ROM* - the core starts with ROM-B (the Omega kit);
     ОСА and Rodionov's system want ROM-A, `firmware/ms0515-roma.rom`;
   - *Keyboard* - the МС 7004 driven by the USB keyboard (by character
     or by key position), a serial keyboard on the user port or the HPS
     UART, or none;
   - *D70.15* - "On GND" is the boards' fix and draws the picture's left
     edge whole; "As drawn" is the schematic as drawn;
   - *Frame rate* - 72 Hz as on the boards photographed; 50 Hz is the
     rate the RT-11 kits' clock counts (at 72 Hz their TIME runs 1.44
     times fast); 60 Hz is the third jumper setting;
   - *RAM disk module* - the 512 KB module in the XS1 slot, in or out;
   - *Reset*.
5. What to check first and against what: `docs/plan.md`, 7e, "The first
   hour on a DE10-Nano".

## The boards: `*-gerbers.zip` and `*-assembly-top.pdf`

1. Upload the zip as it is to a PCB fab: it holds every copper layer,
   both masks and silkscreens, the outline and the drill files (plated
   and bare apart).
2. The order's parameters for НС4 (8 layers, 290.1 x 230.1 mm, 1.6 mm
   FR-4, the layers' order, holes, finish) are in `docs/pcb/fab.md`,
   "What to order"; the module is 4 layers, GND and +5 V inside.  The
   fab's own viewer shows the outline and layer count to check against.
3. Before ordering: `docs/pcb/fab.md`, "Before ordering", the parts to
   hold against the footprints; the module's DRC still has 33 courtyard
   errors from the photos' placement, to be checked against the photos.
4. Soldering: the assembly PDF shows each part's place and name; the
   parts the standard build leaves out are crossed out, and the hand
   changes (W1, R81, the lifted pins) are in `docs/pcb/fab.md`,
   "Assembly".  The parts list: `docs/pcb/bom.md`, where to get them:
   `docs/pcb/sourcing.md`.

## Making them again

After a change to `rtl/`, `boards/de10nano/` or the boards in `kicad/`,
make the files again for the next release (the commands write into
`build/`, which is not kept), name the assembly drawings
`<board>-assembly-top.pdf`, and attach the five files to a release
tagged on `main` (`gh release create vX.Y FILES...`).

- **The core** - Quartus Prime Lite 17.0.2, a clone with its submodule
  (`git clone --recursive`, or `git submodule update --init`), then from
  the repository's root `quartus_sh -t boards/de10nano/core.tcl` (about
  20 minutes).  The result is `build/mister/output_files/ms0515.rbf`;
  it is good when `ms0515.sta.rpt` there shows a positive setup slack
  for every clock.
- **The Gerbers** - KiCad 10 (its `kicad-cli` as `$KICAD_CLI` or on the
  PATH) and Python 3: `python tools/fab.py` for НС4 and `python
  tools/fab.py kicad/ext.kicad_pcb` for the module write
  `build/fab/<board>/<board>-gerbers.zip` and `assembly-top.pdf` (here
  `<board>-assembly-top.pdf`).
