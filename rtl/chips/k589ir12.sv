// К589ИР12 (8212): an 8-bit input/output port - eight latches with
// three-state outputs and a service request (Intel 8212 datasheet).  The
// device is selected while DS1 (pin 1) is low and DS2 (pin 13) high.  With
// MD (pin 2) high the outputs are always on and the selection clocks the
// latches; with MD low the selection turns the outputs on and STB (pin 11)
// clocks them.  A clocked latch lets its input through, and holds when the
// clock falls; CLR (pin 14) low clears the latches while they are not
// clocked.  The service request: INT (pin 23) falls as STB falls, and rises
// again as the device is selected or cleared.  The latches go through the
// master tick as the board's other latches do (К555ИР22).
module k589ir12 (
    input  logic clk,
    input  logic p1,  input  logic p13, input  logic p2,           // DS1 DS2 MD
    input  logic p11, input  logic p14,                            // STB CLR
    input  logic p3,  input  logic p5,  input  logic p7,  input  logic p9,   // DI1..DI4
    input  logic p16, input  logic p18, input  logic p20, input  logic p22,  // DI5..DI8
    output logic p4,  output logic p6,  output logic p8,  output logic p10,  // DO1..DO4
    output logic p15, output logic p17, output logic p19, output logic p21,  // DO5..DO8
    output logic p4_oe,  output logic p6_oe,  output logic p8_oe,  output logic p10_oe,
    output logic p15_oe, output logic p17_oe, output logic p19_oe, output logic p21_oe,
    output logic p23                                               // INT
);
    logic       ds, clocked, on, stb_was, request;
    logic [7:0] d, held;

    assign ds = !p1 && p13;
    assign clocked = p2 ? ds : p11;
    assign on = p2 || ds;
    assign d = {p22, p20, p18, p16, p9, p7, p5, p3};
    always_ff @(posedge clk) begin
        stb_was <= p11;
        if (clocked)
            held <= d;
        else if (!p14)
            held <= '0;
        if (!p14 || ds)
            request <= 1'b0;
        else if (stb_was && !p11)
            request <= 1'b1;
    end
    assign {p21, p19, p17, p15, p10, p8, p6, p4} = held;
    assign {p21_oe, p19_oe, p17_oe, p15_oe, p10_oe, p8_oe, p6_oe, p4_oe} = {8{on}};
    assign p23 = !request;
endmodule
