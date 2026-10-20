# Plan

Each stage ends with its tests green; nothing is wired before it is tested.

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
4. **КР1807ВМ1.**  Pin-level T-11 from the Engineering Specification:
   microcycles, bus timing, interrupt sampling; tested by single
   instructions, then in lockstep with the emulator's CPU.
5. **The other large chips.**  КР580ВИ53, КР580ВВ51, КР580ВВ55,
   КР1818ВГ93, memories - each tested against its datasheet's cycles.
6. **The board in simulation.**  The generated board boots ROM-B to its
   prompt in Verilator; the screen compared with the emulator's frame,
   pixel for pixel.
6b. **Disputed places decided by software.**  Every place the sources do
   not settle becomes a switchable change in `netlist/rev3a.csv` with its
   candidates (as Э3, as the copper, to pin X or Y).  The board is built
   with each and runs the real software - the ROM self-test, Omega and
   ОСА with the RAM disk, floppy writes, Rodionov's system on ROM-A - frame
   by frame against the emulator.  A wrong candidate breaks something; if
   none does, the place does not matter to the machine and is marked so.
   No candidate yet: the wires read so far all match Э3.
6a. **The memory expansion module** (RAM disk + two RS-232C) the same
   way: its connection tables (`docs/ext` of `$MS0515_DATA`) into a netlist
   of its own, its chips as modules, wired to the board's XS1.
7. **The FPGA.**  Tang Nano 20K: master clock, RAM in the on-chip SDRAM,
   HDMI out, disk images from the SD card, the keyboard line on a pin
   (the PC first, a real МС 7004 later).
