# The board's one clock: the 120 MHz master tick (docs/plan.md stage 7a);
# the processor's two-tick paths are in kr1807vm1.sdc.
create_clock -name clk -period 8.333 [get_ports clk]
derive_clock_uncertainty
