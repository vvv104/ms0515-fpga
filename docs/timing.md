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
  never comes out.
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
  PROM D13's table counts on it (its reset at 256 + 4 is line 260).
- **Power-on.**  D76 (КР580ВВ55) is undefined until СТАРТ resets it; its
  model starts with port B an output at 0, which holds РВК1 low and gives
  D13 the 50 Hz page, so the first frames are not 260 lines.  The tests
  measure the frame after the reset.

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
