// К555АП6 (74LS245): eight bus transceivers, three-state, the memory
// expansion module's (plan 6a).  With G (pin 19) low, DIR (pin 1) high
// drives the B side from the A side, low the A side from the B side; G
// high isolates both (TI SN74LS245 SDLS146B table 1).  Both sides are bus
// pins: each has its level in (pN_in), its driven value (pN) and enable.
module k555ap6 (
    input  logic p1, p19,                                          // DIR, G
    // A1..A8 (pins 2-9)
    input  logic p2_in, p3_in, p4_in, p5_in, p6_in, p7_in, p8_in, p9_in,
    output logic p2, p3, p4, p5, p6, p7, p8, p9,
    output logic p2_oe, p3_oe, p4_oe, p5_oe, p6_oe, p7_oe, p8_oe, p9_oe,
    // B1..B8 (pins 18-11)
    input  logic p18_in, p17_in, p16_in, p15_in, p14_in, p13_in, p12_in, p11_in,
    output logic p18, p17, p16, p15, p14, p13, p12, p11,
    output logic p18_oe, p17_oe, p16_oe, p15_oe, p14_oe, p13_oe, p12_oe, p11_oe
);
    logic a_to_b, b_to_a;

    assign a_to_b = ~p19 & p1;
    assign b_to_a = ~p19 & ~p1;
    assign {p18, p17, p16, p15, p14, p13, p12, p11} =
           {p2_in, p3_in, p4_in, p5_in, p6_in, p7_in, p8_in, p9_in};
    assign {p2, p3, p4, p5, p6, p7, p8, p9} =
           {p18_in, p17_in, p16_in, p15_in, p14_in, p13_in, p12_in, p11_in};
    assign {p18_oe, p17_oe, p16_oe, p15_oe, p14_oe, p13_oe, p12_oe, p11_oe} = {8{a_to_b}};
    assign {p2_oe, p3_oe, p4_oe, p5_oe, p6_oe, p7_oe, p8_oe, p9_oe} = {8{b_to_a}};
endmodule
