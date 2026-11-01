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
       **Done** (`boards/de10nano/synth.tcl`): 14 % of the ALMs, 19 % of
       the block memory, 120 MHz met in every timing model (Fmax 125-130
       MHz).  It took the delay lines' "held for the delay" registered a
       tick ahead, the LSI chips' read data a tick after the strobe
       (inside their tRD), the processor's core acting on the bus's events
       two ticks late (they come three apart) with two ticks for its
       arithmetic, and the ВИ53's next count worked out a tick ahead - none
       of it changing what the chips' pins do beyond their datasheets.
       The К573РФ4s then read their cells a tick after the pins (the
       tick taken off tACC, tOE, tDF, as for any clocked chip) and went
       back to M10K: 10 % of the ALMs, 21 % of the block memory, 120 MHz
       met in every model with less to spare (Fmax 120.6 MHz slow 100 C,
       123 MHz slow -40 C).
   7b. **Margins.**  The board booted with every chip at its datasheet's
       minimum and at its maximum delay, and from random power-up states
       under load (Omega and ОСА to their prompts) - a board that runs
       only on lucky delays would not on the FPGA.
       **So far** (`docs/timing.md`, "Corners"): both corners pass every
       test; Omega and ОСА reach their prompts in both.  At the min
       corner the screen is clean.  At the max corner some frames show
       spoiled letters, one frame each, a different place each time.
       Found (`docs/timing.md`, "Corners"): when the video's request
       waits for the processor's memory cycle, its late ЧТ ВЗУ still
       resets D98.1 as the next СТРОБ comes, the request is lost and the
       registers load the word before again - 251 of 1.2 million
       requests in 1.3 s, every spoiled frame.  Typical delays keep 75 ns
       of margin there; with every chip at its slowest there is none: the
       design's, kept.
       From random power-up states (seeds 1-3) Omega reached its prompt
       at typ and at max, ОСА at max too, every DRAM read as written
       (but the cell 000:000 read before the first refresh after
       power-on); at typ one request lost in each 25 s run (seed 2's at
       0 s, in the power-up state; the others' moment is not kept), at max about 5300 a second while the disk is
       read and the screen scrolls (ОСА: 108 013 in 17 s) - the same
       margin, the screens' letters spoiled where it bites.  Done.
   7c. **The bench's parts in RTL.**  What the C++ bench plays becomes
       logic: the МС 5305's MFM stream for the ВГ93 from a disk image
       (MiSTer's SD block interface), the МС 7004's line from MiSTer's
       keyboard (its scan codes to LK201 codes), the video DAC's levels
       to MiSTer's video interface (15 kHz, the scaler does HDMI); each
       tested on the model against the C++ part it replaces.
       **So far** (`rtl/parts/`): the video - `ns4_video.sv` puts the
       board's lines out a line late, a pixel enable per sample, so that
       the frame's blanking, on one wire with the line's on the board, is
       told apart; from power-on every pixel matches the camera's sample.
       The drive, reading - `mc5305.sv` loads the track under the head
       from MiSTer's block device and makes its bytes and their MFM as
       they pass, tick for tick the bench's drive once settled after a
       step (3 ms here, of the 15 ms the controller waits).  Writing in
       both, the bench's drive first (it took ДЗП and kept nothing): the
       falls become cells by their spacing, read as the controller reads
       them, a data field with its CRC right into its sector - the ID's
       before it, or by its place in the layout; a sector written, a
       whole track (Write Track), a protected disk.  On the board
       (`run_board drive=logic`, the bench's drive beside it as the
       reference): Omega from power-on to its prompt, 2420 blocks read
       and 4 written, the two drives' lines apart only while a track
       loads after a step.
       The keyboard: the МС 7004 itself, read from its own documents
       (`docs/ms7004.md`) as the board was - its netlist, its 8035
       (`docs/km1816ve48.md`) and the other chips, the keyboard generated
       and run on its own firmware: it beeps, scans, answers ROM-B's
       question and sends its keys as its ТО says.  In the core it stands
       at XS2 on its cable, MiSTer's USB keyboard pressing its matrix
       (`rtl/parts/ms7004_keys.sv`: by character - РУС/ЛАТ and ВР
       pressed as a character needs - or by position).  A keyboard
       outside stays possible: on a serial line as the emulator takes one
       on a COM port (portboard, or a real МС 7004 through a level
       shifter).
   7d. **The core.**  Template_MiSTer's `sys/` unchanged - a git
       submodule, not copied in: no third-party HDL in the tree - the board and
       the parts of 7c under `emu`, the menu: disk images, ROM-A/ROM-B,
       the rev3a changes and the undecided places as switches.  Built,
       timing closed; the post-fit netlist run on the bench against the
       emulator as the RTL is.
       **So far**: the machine for the core (`rtl/ms0515.sv`, the board
       as `rtl/board/ns4_machine.sv` with only the ports it uses) holds
       tick for tick to the bench's board (`test_machine`); the core
       builds (`boards/de10nano/core.tcl`, an .rbf) and meets timing in
       every domain: 29 % of the ALMs, of the block memory and of the
       DSP blocks; the 120 MHz domain with +0.06 ns (the template's
       single timing model, as MiSTer's cores are built).  It took the
       drive's logic taken apart over ticks (it missed by 6.4 ns, never
       timed before) and the pixels handed to sys/ on a 60 MHz clock from
       the same PLL (`rtl/parts/video_half.sv`) - sys/'s scaler and video
       measurement do not run at 120 MHz; with these out of the way the
       board's chips met timing beside sys/ as well.  With the МС 7004
       inside (7c) it meets timing again: 33 % of the ALMs, 29 % of the
       block memory, the 120 MHz domain with +0.3 ns - the 8035's paths
       two ticks (`km1816ve48.sdc`: its registers change only on its
       crystal's edges, 13 or 14 ticks apart), the translator's tables
       looked up a tick ahead, the drive's layout setting through a
       register.  The menu: the disk image and its layout, the keyboard
       (the МС 7004 with MiSTer's USB keyboard, by character or by
       position; a serial one on the user port or the HPS UART; none),
       the aspect, reset.  Left for 7d: ROM-A in the menu and the rev3a
       switches; the post-fit netlist on the bench.
   7e. **On the board** (whoever has one): power-on, the ROM's self-test,
       Omega and ОСА from SD images, the keyboard, the picture on HDMI and
       on the IO board's VGA; then what is too long for the model -
       minutes of work, games, the RAM disk.  An FPGA is reprogrammed at
       will: nothing done here is final, a fix is a rebuild.
   No SDRAM board is needed while the machine's memory fits the block
   memory (128 KB RAM, 16 KB ROM, the VRAM inside it): DE10-Nano, a USB
   hub and a microSD card are the whole kit; the IO board adds analog
   video for a CRT.
