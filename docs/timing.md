# Timing: the chips' delays

The board model runs on a 120 MHz master clock, a tick of 8.33 ns.  A
chip's output does not change the moment its inputs do: it changes after
the chip's propagation delay, as on the board.

## The rule

- Every chip's outputs pass through a **delay line** that `tools/board.py`
  puts after them: round(delay / 8.33 ns) ticks.  The lines of one delay
  share a bank (`rtl/chips/delay_bank.sv`), up to 64 bits handled at once.
- The delay is the chip's **typical** tPLH/tPHL from its datasheet
  (`netlist/delays.csv`, each row with its source).  Where a datasheet
  gives maximums only (К555ИР16, КР1533ТМ2, К573РФ4Б, КР565РУ5Г), the
  maximum is used and the row says so.
- A **clocked** chip's model already shows its new state a tick after the
  edge, so its line is a tick shorter.
- A **three-state output's enable** has its own delays: turn-on (tPZH/tPZL,
  an EPROM's tOE, a DRAM's tCAC) and turn-off (tPHZ/tPLZ, an EPROM's tDF, a
  DRAM's turn-off), columns `oe_ns` and `off_ns`.
- The delay is **inertial**, as a gate's is: an output takes a level once
  the input has held it for the whole delay; a pulse shorter than the delay
  never comes out.  Whether it has held is worked out a tick ahead and
  registered, so the line's output is a gate from flip-flops - what lets
  the FPGA run it at 120 MHz (`sim/chips/test_delay_bank.cpp` holds it to
  the rule on random input).
- The LSI chips (КР1807ВМ1, КР580ВИ53, ВВ55, ВВ51, КР1818ВГ93) keep their
  own timing from their datasheets inside their models: `model` in the
  table.
- One delay per chip is a simplification: where a chip's paths differ (a
  multiplexer's select and data, a ripple counter's stages) the table
  names the path taken.

## Why: what zero-delay gates broke

With every gate at 0 ns and every flip-flop at a tick, the model ran gates
faster than flip-flops - the reverse of the board, which counts on gates
taking time:

- **ВК4 after ВК3.**  D71's second gate repeats `~D71.8` as СТРОБ H (ВК4,
  the MODE of the video shift registers К555ИР16) one gate later than
  `~D71.8` itself (ВК3, their clock): MODE is still high as the clock falls,
  and the registers load.  At 0 ns the two fell together, the registers
  shifted instead, and the screen stayed black.
- **РАЗР ОЗУ L and D24.**  The NAND D31 takes ПВАС through the inverter D15
  (К555, ~10 ns) and through the flip-flop D24 (КР531, ~6 ns): the fast
  series flip-flop turns off before the slow inverter turns on.  With the
  flip-flop a tick behind the gate the NAND glitched, wrote a one-tick
  -WE into a video cycle and spoiled video cells.

## What the board shows with the delays

- **ROM reads.**  К573РФ4Б puts its data out tOE (150 ns) after ПВАК L, in
  time for the processor's latch; after the read its outputs float within
  tDF (130 ns at most), while the processor drives the next address at its
  typical time: an overlap of a tick or two.  `sim/board/test_board.cpp`
  counts such **handovers** (two drivers on a net for at most 3 ticks) and
  fails on anything longer.
- **The frame counter.**  D41 (АПК8) is clocked by D36's ripple carry,
  which rises as D36 alone reaches 15: АПК8 sets at line 240, and the sync
  PROM D13's table counts on it.  On the 50 Hz page (E3 and E4 both
  fitted, as the machines in use had them - `netlist/variants.md`) the
  frame is 312 lines, 19.97 ms; on the 60 Hz page of the drawing's E4
  alone it was 260 (the reset at 256 + 4).
- **Power-on.**  D76 (КР580ВВ55) is undefined until СТАРТ resets it; its
  model starts with port B an output at 0.  With both jumpers the page
  lines are grounded whatever port B does, so every frame is 312 lines
  from the first; the tests measure the frame after the reset all the
  same.

## The chips as fitted

ПЭ3 allows КР1533 for К555, and the plant used it: the boards
photographed (`netlist/fitted.csv`, `fitted-others.csv`) carry КР1533 at
the five ИЕ10, both ЛА3, the ТМ2/ТМ8/ТМ9, ИР22/ИР23, ЛН1, ЛИ1, ИД7, АП3,
КП15, ЛР11, ЛА2 (and more on the 1991 run).  `tools/board.py corner
<corner> DIR netlist/fitted-1991.csv` generates the board with each such
chip's own delay where `delays.csv` has a row for it (КР1533ИЕ10 11.5 ns
and КР1533ЛА3 6 so far, the midpoints of TI's ALS min and max, which
print no typical), ПЭ3's type's delay otherwise, with a warning.  On the
640 x 200 edge's race it took 7.5 ns off a ~28 ns deficit - not the
answer (`docs/findings.md`, 6b, "The chips as fitted").

## Corners

The board must work with every chip anywhere between its fastest and its
slowest, as the real boards did, each with its own chips (plan stage 7b).
`tools/board.py corner max|min DIR` generates the board at a corner of
the delays; `conan build . -o "&:corner=max"` builds and tests it apart,
in `build/max` (`build/min`):

- **max**: the datasheets' maximum tPLH/tPHL (tPZH/tPZL, tPHZ/tPLZ) of
  the same path from the same document, `netlist/delays-max.csv`, each row
  with its page.  К554СА3А and КР1102АП15 print no maximum and keep their
  typical delay.  The `max` rows of `delays.csv` are maximums already.
- **min**: a printed minimum where a datasheet has one (КР1533ТМ2's 5 ns,
  К573РФ4Б's tDF of 0); elsewhere **half the typical delay** - an
  assumption, the LS datasheets printing no minimum.
- The LSI models keep their own timing in every corner.

What the corners show (Omega booted to its prompt in all three):

- **The video's request at max: lost now and then.**  The video asks for
  its next word once in 1.067 us (128 ticks): СТРОБ H, as the shift
  registers load (ВК4 H), clocks D98.1 (ТРВЗУ H); the word's read ends it,
  ЧТ ВЗУ H resetting D98.1 through D95 (СБРВЗУ L).  When the request
  comes while the processor holds the memory (РАЗР ОЗУ L low), the video
  cycle waits for it to end - АРБ (D73), D43, D47, ЧТ ВЗУ (D23) - and
  runs late.  At the maximums that late ЧТ ВЗУ still holds D98.1 reset
  when the next СТРОБ rises; the reset wins, the request is lost, the
  word is not read, and the latches D51, D57 give the registers the word
  before again: eight points of the screen show the eight before them,
  for one frame.  Omega at max, 20.0 to 21.3 s: 251 of 1 218 750
  requests lost - all 502 loads of a word loaded before, and the spoiled
  letters of frames 1227 and 1250, which are the only frames that differ
  from the one before them.  At the typical delays no request was lost
  in 6 s of the boot, the closest strobe coming 9 ticks (75 ns) after a
  reset ended; at min, 10 ticks.  So the board has the margin with its
  typical chips and none with every chip at its datasheet's slowest -
  a property of the design, kept as it is (the board is not changed to
  pass a corner no board had).  The FPGA runs the typical delays.
- **The frame's start at max.**  СБРАВ H clears the video address
  counters at the frame's start during a video read; at the maximums the
  cleared address reaches А0..7 (through the КП2s) after -RAS or -CAS
  has taken it, so the DRAM reads cell 243:160 or 243:170 for 003:160 or
  243:160 - about one read a frame, in the blanking: nothing on the
  screen.  None at typ and min.
- **The picture at a corner: the emulator's within one sample.**  The
  bench takes the picture's sample at each rise of ТС15 H (the board's
  own 15 MHz; `sim/board/frame_grab.h`), and the board's outputs to the
  video DAC do not change together.  Measured on `vid320.rom`, 6 ms of
  picture, the changes of СЦ, КЦ, ЗЦ H and D69.9 by the master tick after
  the rise they fall on (`run_board ... edges=`):

  | Corner | The largest group (7457 changes) | The second (1164..1241) | The rest (some 240) |
  |---|---|---|---|
  | min | tick 1 | tick 0 | ticks 5, 6 |
  | typ | tick 0 | tick 6 | ticks 2, 4 |
  | max | tick 4 | tick 1 | tick 6 |

  The second group comes ahead of the first by 1 tick at min, 2 at typ,
  3 at max (8, 17, 25 ns).  At typ the sample at the rise takes both
  changed, at max neither; at min the rise is the very tick of the
  second group and the first comes a tick after it, so the sample takes
  the one new and the other old.  On the frame it is single columns of samples: at
  min against typ the border's and the picture's edges (samples 168,
  232, 872, 936) and in 320 x 200 every 16th sample - a word's boundary,
  where the attribute changes; by that the second group is the
  attributes' and the first the points'.  The picture tests, brought on
  2026-11-03 and run at typ only, failed at min from their first day by
  just this (4222 samples in 320 x 200, 100 in 640 x 200).  On a monitor
  nothing is sampled: the edges stand 8 to 25 ns apart, whatever the
  corner - under half a 640 x 200 point (67 ns).  So a board at a corner is held to
  the emulator's picture within one sample - its point and its attribute
  each taken there or a sample to the left or right, where the picture
  fits to the sample best (`sim/board/screen_compare.h`, `slack`); the
  typical board to the sample.
- **A flashing word's inversion outlasts it by 25 ns** (typ).  The
  colour bits go from the attribute registers D55, D56 to the inputs of
  the multiplexers D63, D69 (К555КП2); the selection between ink and
  paper, ВК8 H, comes from D55's БЛНК through D31 (К555ЛА4, with ТМ H
  and РАЗР L) and D39 (К555ЛП5, the XOR with the point ДПСЛ H), two
  gates later.  After a flashing word in its
  inverted half the next word, if it does not flash, shows its ink for 3
  master ticks before its paper, and the bench's sample at the ТС15 H
  rise falls there: single samples against the emulator's picture at
  typ, where the ROM's flashing «Клавиатура не готова» and its cursor
  end (`docs/baseline.md`, open list 3; measured with `edges=`).  ТМ H
  is D59's count of the frame sync by 16: the flash turns every 8
  frames.
- **The processor at max is some 4 % slower from the dynamic RAM** - at
  a system's prompt 4.4 % fewer fetches a second than at typ (min: 0.65
  % more), none fewer through the ROM's start.  Instructions there take
  a part of a microsecond more, of the size of a READY slip; measured
  to the microsecond only, the slips not counted (`docs/baseline.md`,
  open list 10).

The test loads of the datasheets differ (LS 2 kOhm 15 pF; LS three-state
667 Ohm 45 pF, but the LS251's 2 kOhm 15 pF; the open-collector 7416 and
7407 at 110 Ohm 15 pF; S 280 Ohm 15 pF); `delays-max.csv` keeps each
row's load.

## The cost

The 469 delay lines made the model about 1.6 times slower (bench_board:
181x -> 289x slower than the machine); in 16 banks, a machine word at a
time, 253x.  This is the price of the 8.33 ns step: the board relies on
its gates' delays, and 120 MHz is the least clock both crystals (8 and
15 MHz) divide.  Real time is the FPGA's; the simulation is the check,
its long runs run side by side (tools/run_many.py).

Verilator's threads make it slower, not faster: the threads meet at every
master tick, and the meeting costs more than the share of a tick's work
they take off each other (bench_board: one thread 253x, two 375x, four
625x slower than the machine).  Runs side by side, each on one thread,
scale with the cores instead: eight runs take as long as one.
