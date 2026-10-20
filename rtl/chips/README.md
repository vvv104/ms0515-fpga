# Chip modules

One module per chip type, named after it (`k555la3` for К555ЛА3), its
ports the chip's pins:

- a port `pN` for every signal pin N; power pins are left out (they are
  the board's planes);
- a three-state output has a second port `pN_oe`: 1 while the chip drives
  the pin.  The board resolves a bus from the drivers' values and enables;
- an open-collector output gives the level it pulls to: 0, or 1 when it
  lets go.  The board ANDs the open collectors on a net with its pull-up;
- a chip that keeps state has `clk`, the master clock.  Everything runs
  on it (see `CLAUDE.md`): a clock pin of the chip is sampled at every
  master edge, and the chip acts on the pin's edge as seen there.  The
  master clock is many times the board's, so the order of the board's
  edges is kept; a ripple counter changes all its bits at once on its
  clock edge (its internal ripple delay is not modelled).  Asynchronous
  inputs (set, reset, clear) act on the outputs at once and on the
  stored state at the next master edge.

The behaviour of each is its datasheet's function table (the TI 74xx
equivalent named in `netlist/pinouts.csv`), and each has its test in
`sim/chips/test_<module>.cpp`.  Types that differ only in speed share one
module (КР531ТМ2 and КР1533ТМ2 wrap К555ТМ2).
