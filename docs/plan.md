# Plan

Each stage ends with its tests green; nothing is wired before it is tested.

**The emulator is a check, not a source.**  The board is built only from
the primary sources - the netlist read off Э3, each chip's datasheet, the
DEC documents for the processor, the ТО - and nothing in it comes from the
emulator, so where the two agree, two models of different sources confirm
each other.  Where they differ, the documents decide, not the emulator:
- first find out from the documents which one is right;
- the board changes only where it is wrong by the documents;
- otherwise the difference is the emulator's, and goes to its list
  (`docs/t11-model.md`, "Where the emulator differs from the documents")
  rather than into the board.

Three places need care:
- a comparison that forgives (run_board's lockstep waits out the board's
  loops where the board, as the machine does, waits longer than the
  emulator) says how much it forgave, and the screen, compared point for
  point, stays the final check;
- the screen comparison's picture rules come from the emulator's
  documentation (`docs/hardware/video.md` there): a difference in them is
  decided by the ТО, not by bending the board;
- where the emulator departs from Э3 on purpose (the interrupt priority it
  moved after OSA's ^Q hung), the board follows Э3, and a difference in
  behaviour there is a finding for the open question, not a fault.

1. **Netlist of НС4.**  Read the four Э3 sheets and the ПЭ3 element list
   into `netlist/` (parts, pins, nets, with sheet and zone), starting from
   the method of the МС 1702 read-out.  Check the nets against the copper: Mick's
   per-layer traces (layers 1, 2, 7, 8) - or the P-CAD database, if Mick
   gives it, which is the copper's netlist outright - and the rev. 3А
   bare-board scans; every disagreement goes to `docs/findings.md`.
   Generate a KiCad schematic from it to compare with the scans by eye.
2. **Test bench.**  Verilator + C++ (Conan, CMake), the emulator's
   `src/core` linked as the reference model, lockstep after every
   instruction.
3. **Small-scale chips.**  One module per К555/К1533/К531 type used on the
   board, each with its function-table test.
3a. **The board from the netlist.**  `tools/board.py` writes
   `rtl/board/ns4.sv`: the chips with a module as instances, the others
   and the connectors brought out as ports for the test bench, every net
   resolved from its drivers and a board output; every analog part decided
   in `netlist/analog.csv` (`docs/analog.md`).  Tested on made-up boards,
   then the real one: the clock nets from the two crystals, no contention.
   Each later module moves its chip inside.
4. **КР1807ВМ1.**  Pin-level T-11 from the Engineering Specification:
   microcycles, bus timing, interrupt sampling; tested by single
   instructions, then in lockstep with the emulator's CPU.
5. **The other large chips.**  КР580ВИ53, КР580ВВ51, КР580ВВ55,
   КР1818ВГ93, memories - each tested against its datasheet's cycles.
6. **The board in simulation.**  The generated board boots ROM-B to its
   prompt in Verilator; the screen compared with the emulator's frame,
   pixel for pixel.  First the power-on reset: D16 counts `АПК7 Н` of the
   video counters (clocked by `ССИ L` of the РТ12 PROM D21) to give the
   `СТАРТ Н` pulse, and after it no net may have two drivers on at once
   (`contention_at` names the net).
6b. **Disputed places decided by software.**  Every place the sources do
   not settle becomes a switchable change in `netlist/rev3a.csv` with its
   candidates (as Э3, as the copper, to pin X or Y).  The board is built
   with each and runs the real software - the ROM self-test, Omega and
   ОСА with the RAM disk, floppy writes, Rodionov's system on ROM-A - frame
   by frame against the emulator.  A wrong candidate breaks something; if
   none does, the place does not matter to the machine and is marked so.
   No candidate open: the bare wire on the owner's board (GND to
   the lifted D70.15) is settled into `rev3a.csv`; the factory wires all
   match Э3.
6a. **The memory expansion module** (RAM disk + two RS-232C) the same
   way: its connection tables (`docs/ext` of `$MS0515_DATA`) into a netlist
   of its own, its chips as modules, wired to the board's XS1.
7. **The FPGA: a MiSTer core** (decided 2026-10-22; the Tang Nano 20K of
   the first plan dropped).  The target is MiSTer on Terasic's DE10-Nano
   (Cyclone V 5CSEBA6, about 110K LE, 5.5 Mbit of block memory): the
   platform the retro cores live on, whose framework (`sys/` of
   Template_MiSTer) gives the HDMI scaler, USB keyboards, disk images
   from the SD card and the menu.  The core is built with Quartus Prime
   Lite 17.0.2, as MiSTer's cores are.  Whoever buys the board gets a
   core already checked; everything up to 7e runs without one.
   7a. **Synthesis.**  `ns4.sv` alone through Quartus for the 5CSEBA6:
       what does not synthesise, the resources (the delay lines' flops,
       the memories in M10K), and timing at the 120 MHz master clock -
       the chains of fast gates that go straight through, with no
       register, are the risk.  Should it not close: a lower master clock
       or more registered lines, and the board re-checked on the model.
   7b. **Margins.**  The board booted with every chip at its datasheet's
       minimum and at its maximum delay, and from random power-up states
       under load (Omega and ОСА to their prompts) - a board that runs
       only on lucky delays would not on the FPGA.
   7c. **The bench's parts in RTL.**  What the C++ bench plays becomes
       logic: the МС 5305's MFM stream for the ВГ93 from a disk image
       (MiSTer's SD block interface), the МС 7004's line from MiSTer's
       keyboard (its scan codes to LK201 codes), the video DAC's levels
       to MiSTer's video interface (15 kHz, the scaler does HDMI); each
       tested on the model against the C++ part it replaces.
   7d. **The core.**  Template_MiSTer's `sys/` unchanged - a git
       submodule, not copied in: no third-party HDL in the tree - the board and
       the parts of 7c under `emu`, the menu: disk images, ROM-A/ROM-B,
       the rev3a changes and the undecided places as switches.  Built,
       timing closed; the post-fit netlist run on the bench against the
       emulator as the RTL is.
   7e. **On the board** (whoever has one): power-on, the ROM's self-test,
       Omega and ОСА from SD images, the keyboard, the picture on HDMI and
       on the IO board's VGA; then what is too long for the model -
       minutes of work, games, the RAM disk.  An FPGA is reprogrammed at
       will: nothing done here is final, a fix is a rebuild.
   No SDRAM board is needed while the machine's memory fits the block
   memory (128 KB RAM, 16 KB ROM, the VRAM inside it): DE10-Nano, a USB
   hub and a microSD card are the whole kit; the IO board adds analog
   video for a CRT.
