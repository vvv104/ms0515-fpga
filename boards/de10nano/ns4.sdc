# The board's one clock: the 120 MHz master tick (docs/plan.md stage 7a).
create_clock -name clk -period 8.333 [get_ports clk]
derive_clock_uncertainty
