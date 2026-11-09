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

**Two boards, one model** (owner, 2026-11-05).  Stages 1-5 are shared:
the netlist read off Э3 and the chips from their datasheets owe nothing
to any one board.  From stage 6 on there are two projects:
- **the owner's board, `vvv104`** (Э3 with `netlist/vvv104.csv`, D70.15
  on GND) - **active**.  Every run since 2026-10-21 was of this board
  (the model's default), so what was found in 6-8 is its groundwork;
  but the model changed under it (К555ИР16's MODE, DA2 as logic), so it
  is taken again in one baseline run on one commit, `docs/baseline.md`,
  and only what passes there counts;
- **Э3's board, `ns4`** - **paused at 6b**, its picture not the
  emulator's.  When 6b is settled it goes on from the owner's board's
  stages, done again where the two differ.

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
   not settle becomes a switchable change in `netlist/vvv104.csv` with its
   candidates (as Э3, as the copper, to pin X or Y).  The board is built
   with each and runs the real software - the ROM self-test, Omega and
   ОСА with the RAM disk, floppy writes, Rodionov's system on ROM-A - frame
   by frame against the emulator.  A wrong candidate breaks something; if
   none does, the place does not matter to the machine and is marked so.
   The factory wires all match Э3; the bare wire on the owner's board (GND
   to the lifted D70.15) is in `vvv104.csv`.
   **Closed with a fitted speed** (owner, 2026-11-09): with C70 modelled
   Э3's board as drawn lost 640 x 200's left edge at the typical delays;
   with the serial chain's speed (`SERIAL_SPEED`, D30/D21/D14/D70 at
   0.55, fitted, not measured) it draws both modes as the emulator at
   every corner; the frame's first word is the board's own (word 8000)
   (`docs/findings.md`, D70; the `video: Э3's board draws 640 x 200`
   test may_fail at the corners).
6a. **The memory expansion module** (RAM disk + two RS-232C) the same
   way: its connection tables (`docs/ext` of `$MS0515_DATA`) into a netlist
   of its own, its chips as modules, wired to the board's XS1.
7. **The FPGA: a MiSTer core** (decided 2026-10-22; the Tang Nano 20K of
   the first plan dropped).  The target is MiSTer on Terasic's DE10-Nano
   (Cyclone V 5CSEBA6, about 110K LE, 5.5 Mbit of block memory): the
   platform the retro cores live on, whose framework (`sys/` of
   Template_MiSTer) gives the HDMI scaler, USB keyboards, disk images
   from the SD card and the menu.  The core is built with Quartus Prime
   Lite 17.0.2, as MiSTer's cores are.  For the owner's board the
   "Groundwork" below is taken again by the baseline (`docs/baseline.md`);
   for Э3's it waits for 6b.
   Whoever buys the board gets a
   core already checked; everything up to 7e runs without one.
   7a. **Synthesis.**  `ns4.sv` alone through Quartus for the 5CSEBA6:
       what does not synthesise, the resources (the delay lines' flops,
       the memories in M10K), and timing at the 120 MHz master clock -
       the chains of fast gates that go straight through, with no
       register, are the risk.  Should it not close: a lower master clock
       or more registered lines, and the board re-checked on the model.
       **Groundwork** (`boards/de10nano/synth.tcl`): 14 % of the ALMs, 19 % of
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
       123 MHz slow -40 C).  The core's build (7d) has taken its place:
       the board is not synthesised alone again.
   7b. **Margins.**  The board booted with every chip at its datasheet's
       minimum and at its maximum delay, and from random power-up states
       under load (Omega and ОСА to their prompts) - a board that runs
       only on lucky delays would not on the FPGA.
       **Groundwork**, the owner's board only (`docs/timing.md`, "Corners"): both corners pass every
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
       margin, the screens' letters spoiled where it bites.
       **Baseline** (2026-11-06, `docs/baseline.md`, steps 5 and 6):
       both corners pass every test, 73 of 73; ОСА (20 s) and Omega (30
       s) reach their prompts at typ, min and max, every DRAM read as
       written; lost requests 0 or 1 a run at typ and min; at max 178 134
       (ОСА) and some 246 450 (Omega) - some 16 500 a second while Omega
       waits at its prompt.  The same on the board after step 6's fix
       (17 runs): 178 210 and some 246 400 at max.  Then two games
       (BIRDS, Saboteur 2 from a DEC RT-11 diskette) stood still at their
       first instructions: the КР580ВВ51А's model held a character
       written before TxEN went off, which ROM-B does to the keyboard,
       and the games wait for TxRDY.  And a character written with TxEN
       off - as ROM-B leaves it, under Omega too - never left the buffer,
       where the machine's own games show TxRDY coming back.  Both
       fixed, with tests, 76 of 76 in the three corners; on the board so
       fixed the 17 boots pass again and both games run at typ, min and
       max - BIRDS to its field and its sound, Saboteur 2 through the
       menu and the briefing to the jump, the landing and its sound, by
       the keypad and by the joystick; it loads for 20 s, a block a turn
       of the diskette (`docs/baseline.md`, open 9).  The cell 000:000 "read not as written" at max
       (above) was the bench's watch, not the board: it took strobes
       standing low at the first tick for a write; fixed, with a test.
   7c. **The bench's parts in RTL.**  What the C++ bench plays becomes
       logic: the МС 5305's MFM stream for the ВГ93 from a disk image
       (MiSTer's SD block interface), the МС 7004's line from MiSTer's
       keyboard (its scan codes to LK201 codes), the video DAC's levels
       to MiSTer's video interface (15 kHz, the scaler does HDMI); each
       tested on the model against the C++ part it replaces.
       **Groundwork** (`rtl/parts/`): the video - `ns4_video.sv` puts the
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
       **Baseline** (2026-11-06, `docs/baseline.md`, step 6): it boots
       Omega to its prompt.  After 8 blocks written its image was not
       the bench drive's - and the core's drive was the one that was
       right: the controller's model ended a sector's write a byte early
       (no FF after the CRC) and the bench's drive then lost a write
       whose CRC ends in a 0.  Both fixed, with tests.  On the fixed
       board Omega's eight writes leave the two drives' images the same,
       their lines apart only while a track loads.
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
       the vvv104 changes and the undecided places as switches.  Built,
       timing closed; the post-fit netlist run on the bench against the
       emulator as the RTL is.
       **Groundwork**: the machine for the core (`rtl/ms0515.sv`, the board
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
       the aspect, reset; Load ROM puts another 16 KB ROM (ROM-A, which
       OSA wants) in the sockets XS3/XS4 - the cells of D65/D77 written
       through the generator's socket ports (`load_*` of a chip) while the
       machine is held in reset (34 % of the ALMs now; the 120 MHz
       domain +0.07 ns, once the processor's core-to-bus paths got the two
       ticks its request stands before the bus takes it - five and more,
       `test_machine`).  Built again on 2026-11-05 (after the key
       detector and the К555ИР16 change) it missed by 0.24 ns, all of it
       the translator's: its eight-word queue, placed in an MLAB, read and
       looked up in the tables in one tick.  The queue's head is now
       registered and looked up a tick later, two ticks after an event
       instead of one: the 120 MHz domain +0.65 ns (33 % of the ALMs).  The owner's board's changes
       (`netlist/vvv104.csv`) are switches: the generator gives each moved
       input an `s_<ref>_<pin>` that takes it back to Э3's net, and the
       menu has one for D70.15 (on GND, or as drawn).  Left for 7d: the
       post-fit netlist on the bench.
       **Baseline** (2026-11-06, `docs/baseline.md`, step 7): built
       again - on the walk's tree 33 % of the ALMs, the 120 MHz domain
       +0.647 ns; after the controller's fix of step 6, 32 % and +0.239
       ns; after the USART's two fixes, 33 % and +0.633, then +0.630
       ns.  The
       post-fit netlist on the bench was tried and does not work with
       Quartus's own cell models (the baseline's open item 4): the
       owner's to decide.
   7e. **On the board** (whoever has one): power-on, the ROM's self-test,
       Omega and ОСА from SD images, the keyboard, the picture on HDMI and
       on the IO board's VGA; then what is too long for the model -
       minutes of work, games, the RAM disk.  An FPGA is reprogrammed at
       will: nothing done here is final, a fix is a rebuild.
       **The first hour on a DE10-Nano**, in order, each against what the
       model showed (2026-11-08; the `.rbf` of `build/mister/output_files/`
       built on the 50 Hz board, `docs/baseline.md` §7):
       1. The kit: DE10-Nano with MiSTer's `sys/` of the submodule's
          revision on its SD (Main, the menu), a USB hub with a keyboard,
          HDMI to a monitor that takes 50 Hz (or `vsync_adjust` in the ini
          for one that does not); the IO board only for a CRT.  `ms0515.rbf`
          into `_Computer/`, ROM-B as the core's default, ROM-A by Load ROM.
       2. Power-on with no diskette: the self-test's words «Электроника ...
          Тест ОЗУ ... Без дефектов ... Клавиатура готова», the keyboard's
          beep over HDMI's sound, then «НГМД не готов».  Time it: the
          board's self-test holds the screen 3.7 s.  A frame is 20 ms -
          the menu's video info should say 50 Hz, 312 lines.
       3. The picture: the border, the 640 x 200 text with its first column
          whole (the owner's board, D70.15 on GND - the menu's switch);
          flip the switch to «as drawn» and look at the first column of
          every line - the model draws it one point right and loses the
          16th (6b); a real board's answer to 6b is a screenshot of this.
       4. Omega from `test_omega.dsk` on the SD: the prompt in 12 s at
          the model's pace; `DIR`, `TIME` (a minute of it against a
          watch - it is the frame's clock), `SET SL ON` and a line edited.
       5. ОСА from `test_osa.dsk` with ROM-A; Mihin's and Rodionov's
          systems, the kits' README says which ROM each wants.
       6. The keyboard: every key by character and by position, РУС/ЛАТ,
          ВР, СУ, the lamps, auto-repeat (264 from the firmware, the ROM
          repeats the key), the click on repeats only.
       7. The diskette: a file written (`COPY`, `INIT` of a scratch image)
          and the image read back on the host with `ms0515-disk dir`;
          Saboteur 2's load timed against the 20 s (a block a turn).
       8. What the model never ran long enough for: an hour at the
          prompt, the RAM disk (the module is 6a, not in the core yet), a
          game played through, and anything that looks wrong noted with
          the frame it happened in - the bench can run the same seconds.
   No SDRAM board is needed while the machine's memory fits the block
   memory (128 KB RAM, 16 KB ROM, the VRAM inside it): DE10-Nano, a USB
   hub and a microSD card are the whole kit; the IO board adds analog
   video for a CRT.
8. **A printed board to build.**  НС4 again in copper, from the netlist
   the model runs on: a new layout (six copper layers: signals on F.Cu
   and In1.Cu, the ground plane on In2.Cu, the +5 V plane on In3.Cu,
   signals on In4.Cu and B.Cu - four were planned, but with the two
   planes honoured the two outer layers left 140 of 1258 connections
   after 12 passes, where four signal layers took the board to 8; chosen
   2026-11-07), not the 1991 one - its inner layers are not on any scan, and
   only the connections have to be the same.  KiCad 10 (`kicad-cli`),
   Freerouting for the routing.  The owner's board first:
   it is routed from the empty board, and Э3's made from it once 6b is
   settled.  The routing begun before was stopped (carried onto a board
   placed again it kept 12 clearance violations): no routing is carried
   across a change of placement.
   8a. **The copper's questions closed.**  Groundwork: every join of the traces
       against Э3 is two outputs on a wire, Э3 right (`docs/findings.md`).
   8b. **What the software so far never touched**, run on the model with
       a test program from a boot disk, the emulator in lockstep and the
       bench watching the connector's pins: the С2 USART (XP5, with a
       loop back), the МС 7007 PPI (XP3, XP8), the ВИ53's sound, the
       memory dispatcher's banks over all 128 KB, the video modes.
       **Done, otherwise than planned** (2026-11-03, commits 61f225c,
       50280ea, 555cf64, 99c7002, 421012f, 672aaf9): not from a boot
       disk but as test ROMs (`firmware/tests/*.mac`, `tools/test_roms.py`)
       run from reset in place of the machine's ROM, the bench on the
       connector each leads to, what the program saw read off its RAM
       (`sim/board/test_parts.cpp`): С2 through a loop-back plug on XP5,
       the МС 7007 port through a plug from XP3 to XP8, the speaker's
       node on XP7 from the timer and from the control bit, the
       dispatcher's banks over the whole 128 KB, the picture in 320 x
       200 and 640 x 200 against the emulator's, and the interrupts of
       С2 and of the МС 7007's key at their vectors.  They pass at typ,
       min and max (`docs/baseline.md`, steps 4 and 5).
   8c. **Footprints and parts.**  Every part of ПЭ3 with a footprint: the
       DIPs and their sockets, the passives, crystals, DT1/DT2, the
       connectors - imported ones where their mates allow (the cables to
       the drive, monitor and supply checked pin for pin); the list of
       parts with the Soviet type, an equivalent, the package.  The
       options (SA1, E3/E4) fitted as jumpers, set for 3.858.420 (the
       МС 7004 on XS2, ROM-B, as the owner's machine).
   8d. **Layout.**  Placement by blocks, the decoupling of ПЭ3, routed by
       Freerouting, DRC to a common fab's rules.  Two boards, one
       placement (owner, 2026-11-05): `ns4` is Э3 as drawn - the one that
       still needs work, its 640 x 200 left edge open (`docs/findings.md`)
       - and `ns4-vvv104` the owner's board as it worked, with
       `netlist/vvv104.csv`'s change (D70.15 on GND), the conditionally
       working one.  No jumper chooses between them: on Э3 a jumper is a
       mode of the machine, not a repair.  The green wire is such a mode
       (the RAM-disk module's), so both boards have it as a jumper, E9
       (`netlist/pcb_parts.csv`, `pcb_nets.csv`, `pcb.csv`: D80.5 on its
       middle contact, 1-2 Э3's `ВБРЕГ L` as delivered, 2-3 `АД03 H`);
       the board's own additions are placed after Э3's parts, so those
       stand where they would without them.  The owner's board is routed
       first, from the empty board; the other is made from it, as placed
       and routed, and only the nets its pins left are routed again
       (`tools/pcb.py vvv104` places it - every part where Э3's nets put
       it, so the two boards have one placement; `tools/pcb.py derive ns4
       vvv104` and `tools/route.py start --from vvv104` then make Э3's).
       The routing runs in chunks that survive a stop (`tools/route.py`);
       a routing is never carried onto a board with a part moved.  The
       owner's board's routing began from the empty board on 2026-11-05
       (1258 connections to route).
       **Baseline** (2026-11-06, `docs/baseline.md`, step 8): it stopped
       with 8 connections unrouted, and is not this board: the design
       given to Freerouting named the plane layers signal layers, and
       more than half the track runs over them, the planes in pieces
       (so did the routing of 2026-11-04).  Nothing of it is committed.
       **The stack settled** (2026-11-07): six layers, the plane layers
       of type power in `tools/pcb.py` (`stack`), which the Specctra
       export writes as `(type power)` and Freerouting keeps its signals
       off; `tools/route.py start` refuses a design with a plane on a
       signal layer.  The narrow tracks were Freerouting's automatic
       neckdown (3/4, 3/5, 1/2 of the width where a connection would not
       go in whole) and the edge's errors its own 0.25 mm to the outline:
       `run` turns the one off and gives it KiCad's 0.5 mm for the other
       (`route.py`'s docstring).  The build is reproducible now - the
       placement followed Python's string hashing before, so each
       `pcb.py` gave another board.  The routing: `route.py vvv104
       --work DIR run 2 5` from a `start` in that folder.
   8e. **The layout against the netlist.**  The netlist read back from
       the routed board (`kicad-cli`) equal to `netlist/`, pin for pin - a
       test, so that the board made is the board simulated.
   8f. **For the fab and the bench.**  Gerbers, drill files, the parts
       list, the assembly drawing, the options and the first power-on.
