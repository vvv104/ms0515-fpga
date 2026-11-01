// pll: the core's clocks for MiSTer's sys/ (it takes the core's PLL from
// rtl/pll.qip beside the project, which core.tcl writes to name this file):
// the machine's 120 MHz master clock and the 60 MHz video clock, in phase,
// from the DE10-Nano's 50 MHz - Intel's altera_pll from Quartus's own
// library.  The hierarchy is the one Quartus's PLL wizard makes,
// pll|pll_inst|altera_pll_i, which sys_top.sdc names to put the core's
// clocks in a group of their own.
module pll (
    input  wire refclk,
    input  wire rst,
    output wire outclk_0,
    output wire outclk_1,
    output wire locked
);
    pll_120 pll_inst (.refclk, .rst, .outclk_0, .outclk_1, .locked);
endmodule

module pll_120 (
    input  wire refclk,
    input  wire rst,
    output wire outclk_0,
    output wire outclk_1,
    output wire locked
);
    altera_pll #(
        .fractional_vco_multiplier("false"),
        .reference_clock_frequency("50.0 MHz"),
        .operation_mode("direct"),
        .number_of_clocks(2),
        .output_clock_frequency0("120.000000 MHz"),
        .phase_shift0("0 ps"),
        .duty_cycle0(50),
        .output_clock_frequency1("60.000000 MHz"),
        .phase_shift1("0 ps"),
        .duty_cycle1(50),
        .pll_type("General"),
        .pll_subtype("General")
    ) altera_pll_i (
        .refclk(refclk), .rst(rst), .outclk({outclk_1, outclk_0}), .locked(locked),
        .fboutclk(), .fbclk(1'b0)
    );
endmodule
