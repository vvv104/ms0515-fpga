// К573РФ4 (2764): an 8K x 8 EPROM, read: the outputs driven while CE (pin
// 20) and OE (pin 22) are low and PGM (pin 27) high.  CONTENTS holds the
// byte at address a in bits 8a+7..8a; the cells start as CONTENTS, and may
// be written through load_* - another chip put in the socket (the board's
// ROMs stand in XS3/XS4): the core loads one from its menu, a test bench
// may also write the cells directly.
//
// The read is registered on the master tick, the outputs and their enables
// a tick after the pins: the FPGA's block memory reads only so.  The tick
// comes out of the chip's delays (tACC, tOE, tDF), as for any clocked chip
// (tools/board.py), so the pins answer when the datasheet says.
module k573rf4 #(
    parameter logic [65535:0] CONTENTS = '1
) (
    input  logic clk,
    input  logic p10, input  logic p9,  input  logic p8,  input  logic p7,   // A0..A3
    input  logic p6,  input  logic p5,  input  logic p4,  input  logic p3,   // A4..A7
    input  logic p25, input  logic p24, input  logic p21, input  logic p23,  // A8..A11
    input  logic p2,                                                         // A12
    input  logic p20, input  logic p22, input  logic p27,                    // CE OE PGM
    // the socket: another chip put in, its cells written a byte a tick (the
    // core loads a ROM image from its menu; the generator brings these out)
    input  logic load_we, input  logic [12:0] load_a, input  logic [7:0] load_d,
    output logic p11, output logic p12, output logic p13, output logic p15,  // D0..D7
    output logic p16, output logic p17, output logic p18, output logic p19,
    output logic p11_oe, output logic p12_oe, output logic p13_oe, output logic p15_oe,
    output logic p16_oe, output logic p17_oe, output logic p18_oe, output logic p19_oe
);
    logic [12:0] a;
    logic [7:0]  q;
    logic        on;
    logic [7:0]  cells [8192] /*verilator public_flat_rw*/;
    initial for (int i = 0; i < 8192; i++) cells[i] = CONTENTS[8 * i +: 8];
    assign a = {p2, p23, p21, p24, p25, p3, p4, p5, p6, p7, p8, p9, p10};
    always_ff @(posedge clk) begin
        q  <= cells[a];
        on <= !p20 && !p22 && p27;
    end
    always_ff @(posedge clk)
        if (load_we)
            cells[load_a] <= load_d;
    assign {p19, p18, p17, p16, p15, p13, p12, p11} = q;
    assign {p19_oe, p18_oe, p17_oe, p16_oe, p15_oe, p13_oe, p12_oe, p11_oe} = {8{on}};
endmodule
