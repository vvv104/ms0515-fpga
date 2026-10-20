// К555АП5 (74LS244): two groups of four buffers, three-state, each group
// enabled by its G low.
module k555ap5 (
    input  logic p1,                                               // 1G
    input  logic p2,  input  logic p4,  input  logic p6,  input  logic p8,   // 1A1..1A4
    output logic p18, output logic p16, output logic p14, output logic p12,  // 1Y1..1Y4
    output logic p18_oe, output logic p16_oe, output logic p14_oe, output logic p12_oe,
    input  logic p19,                                              // 2G
    input  logic p11, input  logic p13, input  logic p15, input  logic p17,  // 2A1..2A4
    output logic p9,  output logic p7,  output logic p5,  output logic p3,   // 2Y1..2Y4
    output logic p9_oe,  output logic p7_oe,  output logic p5_oe,  output logic p3_oe
);
    assign {p18, p16, p14, p12} = {p2, p4, p6, p8};
    assign {p9, p7, p5, p3}     = {p11, p13, p15, p17};
    assign {p18_oe, p16_oe, p14_oe, p12_oe} = {4{~p1}};
    assign {p9_oe, p7_oe, p5_oe, p3_oe}     = {4{~p19}};
endmodule
