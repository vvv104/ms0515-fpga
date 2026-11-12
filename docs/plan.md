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
- **Э3's board, `ns4`** - 6b closed with a fitted speed of the chain to
  D70 (2026-11-09); it goes on from the owner's board's stages, done
  again where the two differ, and its printed board is routed first
  (stage 8).

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
   typ and min; at max 640 x 200's first word is a point off (the race's
   window is narrower than the corners' spread); the frame's first word
   is the board's own (word 8000) (`docs/findings.md`, D70; the `video:
   Э3's board draws 640 x 200` test may_fail at the corners, failing at
   max).
6a. **The memory expansion module** (RAM disk + two RS-232C) the same
   way: its connection tables (`docs/ext` of `$MS0515_DATA`) into a netlist
   of its own, its chips as modules, wired to the board's XS1.
   **Started 2026-11-12** (owner: the same result as НС4's - the module in
   the FPGA beside the board on the one chip, and a printed board to make).
   What there is:
   - the plant's documents in `docs/ext`: the parts list (35 chips: D1
     КМ1818ВГ01, D2 КР580ВВ55А, D3 КР580ВИ53, D4/D5 КР1102АП15, D6 К555АП6,
     D7/D15-D17 К555КП11, D8 К555ТМ2, D9/D11 КР559ИП11, D10 К555ЛН1, D12
     К555ЛЛ1, D13 КР531ИД14, D14 К555ИЕ19, D18 К555КП2, D19-D34 КР565РУ7Г,
     D35 К555ИР16, G1 5.0688 MHz), the connection tables pin by pin and a
     description (512 KB, 1024 blocks, two RS-232C);
   - the owner's module, two photos (`photos/known-machines/0_d497*`,
     1280 px, two layers): it carries the RAM disk's 27 chips only - D2,
     D6, D10, D12-D18, D35, D19-D34 by type - and none of the RS-232
     half (D1, D3-D5, D7-D9, D11, G1).  Its board is not the tables' 245 x
     55 mm one as drawn; the photos are too coarse to trace its copper, so
     the tables are the netlist and the photos check the parts.
   How it joins НС4 (XP1 A n = XS1 n, B n = 32 + n, C n = 64 + n): the
   module decodes its registers with `АД03` high and `выбор устр` low
   (ВВ55 at 177530-536, the RAM's data at 177550 - what EX.SYS uses), and
   takes four signals Э3 does not bring to XS1: C21 ТС15 (XS1.85), C22
   ДЧТ51 (86), C23 выбор устр (87), C24 СТАРТ (88).  On НС4 they are,
   by logic, `ТС15 H`, `ЧТ51 L`, `ВБРЕГ L` (D80's enable) and `СТАРТ H`;
   with them the green wire (`АД03` to D80.5) is what keeps НС4's own
   registers off the module's half.  To be proven on the model.
   The steps:
   1. The module's netlist from the tables (`netlist/ext/`), checked
      against the parts list and the photos; the four XS1 joins in
      `vvv104.csv`.
      **Done** (2026-11-12): `netlist/ext/` - 104 parts, 151 nets, 746
      pins (the chips' power pins in `parts.csv`), every pin the tables
      name on one net and every chip pin they leave out an unused one (D1
      11, 30-32, 38; D11's fourth receiver; D35's QB; G1's free pins);
      `tools/netlist.py check ext` clean.  The joins went to
      `netlist/ext/joins.csv`, not `vvv104.csv` (that file takes only what
      a photo shows): 54 contacts as Э3 wires them, the four wires above,
      10 RS-232-half lines Э3 leaves free (A21-A24, B21-B24, C17, C18) and
      2 in conflict - C19 (цепь 108) and C20 (TxD_B) are GND on Э3's XS1
      (83, 84): the full module's drivers would fight НС4's ground there;
      the owner's module has no D5, so no driver.  `fitted.csv`: the
      owner's module, 27 chips by type and date.  To settle on the model:
      D14, the RAM disk's address counter, is clocked from `ДЧТ51` alone
      (`ТАКТ`), so it would step on reads only; D1's reset is СТАРТ
      inverted, the ВВ55's not.  D1 (КМ1818ВГ01) is wired as an Intel
      8274; no pinout until it is identified.
   2. The missing chip models: К555АП6, КР565РУ7Г (from КР565РУ5's);
      КМ1818ВГ01 and G1 for the RS-232 half, after ВГ01 is identified.
      **The RAM disk's done** (2026-11-12): `k555ap6.sv` (74LS245, TI
      SDLS146B: typ 8 ns, enable 26, disable 15) and `kr565ru7.sv` (41256,
      the eandc К565РУ7 sheet: КР565РУ7Г tCAC <= 100 ns, off <= 70, tRAC
      <= 200), each with its test; 79 of 79 at typ (НС4's board is not
      touched).  ВГ01 and G1 wait for the RS-232 half.
   3. The module generated beside the board (`tools/board.py`), in
      simulation: Omega's EX.SYS finds the RAM disk, a file copied to it
      and read back, against the emulator's RAM disk; doubts in the
      tables settled the 6b way, by the software.
   4. The core: the module on the same FPGA, a switch in the menu.
   5. The printed board: two layers, on XP1 (DIN 41612 C, as НС4's XS1
      is made anew), the RS-232 half fitted or not.
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
   the model runs on: a new layout with the 1991 board's eight copper
   layers (owner, 2026-11-09: signals on F.Cu and In1.Cu, the ground
   plane on In2.Cu, signals on In3.Cu and In4.Cu, the +5 V plane on
   In5.Cu, signals on In6.Cu and B.Cu; the chips where the 1991 board
   had them).  Four layers left 140 of 1258 connections after 12 passes,
   six (chosen 2026-11-07) 309 after two days.  Not the 1991 copper -
   its inner layers are not on any scan, and only the connections have
   to be the same - though with its placement and its layers the
   routing may come out near it.  KiCad 10 (`kicad-cli`), Freerouting
   for the routing.  Э3's board first, as the paper schematic draws it
   (owner, 2026-11-09, with 6b closed): it is routed from the empty
   board, and the owner's made from it.  The routings begun before
   (the owner's board, four and six layers) were stopped: no routing is
   carried across a change of placement or of the stack.
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
       placement (owner, 2026-11-05): `ns4` is Э3 as drawn - its 640 x 200
       left edge closed with a fitted speed of the chain to D70
       (`docs/findings.md`; on copper the chips as fitted decide it)
       - and `ns4-vvv104` the owner's board as it worked, with
       `netlist/vvv104.csv`'s change (D70.15 on GND), the conditionally
       working one.  No jumper chooses between them: on Э3 a jumper is a
       mode of the machine, not a repair.  The green wire is such a mode
       (the RAM-disk module's), so both boards have it as a jumper, E9
       (`netlist/pcb_parts.csv`, `pcb_nets.csv`, `pcb.csv`: D80.5 on its
       middle contact, 1-2 Э3's `ВБРЕГ L` as delivered, 2-3 `АД03 H`);
       the board's own additions are placed after Э3's parts, so those
       stand where they would without them.  Э3's board is routed first,
       from the empty board (owner, 2026-11-09; before, the owner's was);
       the other is made from it, as placed and routed, and only the nets
       its pins left are routed again (`tools/pcb.py` places Э3's -
       every part where Э3's nets put it, so the two boards have one
       placement; `tools/pcb.py derive vvv104` and `tools/route.py vvv104
       start --from ns4` then make the owner's).
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
       `pcb.py` gave another board.  Six layers stopped at 309 of 1258
       left (2026-11-08, ~5 connections an hour at the end).
       **Eight layers** (owner, 2026-11-09), as the 1991 board: six of
       signals, the planes on In2.Cu and In5.Cu.  Э3's board routed from
       the empty board: `route.py start`, `run 8 3` (build/route) - 15
       left, stopped: the owner's word (2026-11-10) is the 1991 board's
       copper, so first every part where the 1991 board had it.
       **The 1991 placement** (2026-11-10): every part on its 1991 holes
       (`netlist/placement-1991.csv`, read off the assembly drawing;
       `docs/findings.md`, "The 1991 placement off the assembly drawing"),
       the footprints of the 1991 holes (`tools/pretty.py`).
       **The 1991 outer copper** (2026-11-10): Mick's layers 1, 2, 7, 8
       laid on F.Cu, In1.Cu, In6.Cu, B.Cu, locked, with the 1991 vias
       (`tools/copper1991.py` into `netlist/lines-1991.json`,
       `tools/lay1991.py`, `pcb.py`): each run takes the net of the hole
       nearest along the copper, the runs where two nets meet (the trace
       read's merges, the factory patches' and E9's pins) are cut, stubs
       on a neighbour's copper taken off.  Both boards: DRC no error.
       What it leaves to route goes inside, `route.py start` keeping the
       locked copper (`(type fix)`).  Twelve two-pin parts turned round
       on the way, where the copper said so (`docs/findings.md`).
       **The inside routed by PathFinder** (2026-11-10, `tools/pathfind.py`):
       Freerouting over the locked copper went slowly (1584 items, 724
       left after two passes, each pass an hour and more).  The routing
       left is the hidden layers' own work - DRC's 499 unconnected
       against the 487 pairs of `docs/pcb/inner-layers.md` - and every
       hole of the 1991 board shows outside, so its inner layers changed
       layer only in holes Mick's scans show.  PathFinder (the FPGA
       router: every net alone, sharing allowed, a shared cell dearer
       round by round) on a 1.27 mm grid, one track between two pins
       (the board's photos: 0.25..0.3 mm), layer changes only in the
       pins, the 1991 vias and the 248 holes no outer copper takes.
       On In3/In4 with the free room of In1/In6: every join made, none
       shared, in some forty rounds (a few minutes); 149 of the free
       holes became vias.  On In3/In4 alone, as the 1991 board may have
       been: 16 cells still shared after 400 rounds.
       **The 1991 runs 0.25 mm wide** (2026-11-10): 0.19 mm was the line
       Mick drew, three pixels; the owner's board photo shows its runs
       0.25..0.3 mm (`docs/findings.md`).  Laid 0.25 mm, narrower only
       where the traces' half-pixel error leaves less than the clearance
       (`lay1991.fit_runs`, a run cut in 2.54 mm pieces, a square pad by
       its sides): 99.8 % of the length at 0.25.  Pieces of copper that
       reach no hole are taken off (`lay1991.drop_scraps`).  Routed inside
       again: KiCad's DRC **no error, nothing unconnected**
       (`build/pathfind/ns4-pathfind.kicad_pcb`; the warnings are the
       silkscreen's, the ends of Mick's runs that go inside, and 11 of
       his vias no net needed).
       **Power, a first check** (2026-11-10, `docs/pcb/power.md`): the
       planes drop 3.7 mV at 2 A; the DRAM bank's edges move them 30..40
       mV; ВГ93's +12 V dips 37 mV with C4 on it and 447 mV without -
       C4 on +12 V, as the copper has it, is the right one.
   8e. **The layout against the netlist.**  The netlist read back from
       the routed board equal to `netlist/`, pin for pin - a test, so that
       the board made is the board simulated.  **Done** (2026-11-10,
       `tools/readback.py`): the net names on the copper are ours, so it
       does not read them - pads, tracks, vias and the planes' fill joined
       where their copper touches (a plane where its fill overlaps a pad:
       a spoke), each group of pins a net of the board as it would be made.
       The routed board (`build/pathfind/ns4-pathfind.kicad_pcb`): 2436
       pins, 16065 track pieces, 819 vias, 2 planes - 635 groups, **no
       difference** from the netlist.  A copy spoiled on purpose (one
       inner run taken off, a run from D14.1 to D14.2 named as D14.1's
       net) is told both: `МК02 H` in 2 pieces, `БОРДС L` and `БОРДК L`
       joined.
   8f. **For the fab and the bench.**  Gerbers, drill files, the parts
       list, the assembly drawing, the options and the first power-on.
       **Done for Э3's board** (2026-11-10, `docs/pcb/fab.md`): the routed
       board is `kicad/ns4.kicad_pcb` (`tools/pathfind.py apply`), checked
       again there (pcb.py check, readback, DRC: no difference, no error,
       nothing unconnected); `tools/fab.py` writes the gerbers, the drill
       files and the assembly drawing; what to order, what to hold against
       the real parts, the options and the first power-on in `fab.md`.
       **The owner's board routed** the same way (2026-11-10,
       `pathfind.py vvv104 ...`, E9 kept): its routing's vias cut R75.2 off
       the GND plane on a piece of fill - KiCad's DRC told it, and so does
       `readback.py` now, which takes each piece of a plane's fill on its
       own; `pathfind.py vvv104 planes` hands such pins to the next
       routing, which joins them by a run.  Then: pcb.py check, readback
       (against the variant's netlist; against Э3's exactly D70.15) and
       DRC no difference, no error, nothing unconnected.  Э3's board had
       no such pin.
