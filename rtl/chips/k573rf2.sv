// К573РФ2 (2716): a 2K x 8 EPROM, read: the outputs driven while CE
// (pin 18) and OE (pin 20) are low.  CONTENTS holds the byte at address a in
// bits 8a+7..8a.  As К573РФ4, the read is registered on the master tick: the
// outputs and their enables a tick after the pins, the tick taken off the
// chip's delays (tACC, tOE, tDF) by the generator.
module k573rf2 #(
    parameter logic [16383:0] CONTENTS = '1
) (
    input  logic clk,
    input  logic p8,  input  logic p7,  input  logic p6,  input  logic p5,   // A0..A3
    input  logic p4,  input  logic p3,  input  logic p2,  input  logic p1,   // A4..A7
    input  logic p23, input  logic p22, input  logic p19,                    // A8..A10
    input  logic p18, input  logic p20,                                      // CE OE
    output logic p9,  output logic p10, output logic p11, output logic p13,  // O0..O7
    output logic p14, output logic p15, output logic p16, output logic p17,
    output logic p9_oe,  output logic p10_oe, output logic p11_oe, output logic p13_oe,
    output logic p14_oe, output logic p15_oe, output logic p16_oe, output logic p17_oe
);
    logic [10:0] a;
    logic [7:0]  q;
    logic        on;
    logic [7:0]  cells [2048];
    initial for (int i = 0; i < 2048; i++) cells[i] = CONTENTS[8 * i +: 8];
    assign a = {p19, p22, p23, p1, p2, p3, p4, p5, p6, p7, p8};
    always_ff @(posedge clk) begin
        q  <= cells[a];
        on <= !p18 && !p20;
    end
    assign {p17, p16, p15, p14, p13, p11, p10, p9} = q;
    assign {p17_oe, p16_oe, p15_oe, p14_oe, p13_oe, p11_oe, p10_oe, p9_oe} = {8{on}};
endmodule
