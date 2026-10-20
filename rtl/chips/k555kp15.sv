// К555КП15 (74LS251): 8-to-1 multiplexer, three-state outputs Y (pin 5)
// and W = not Y (pin 6); OE (pin 7) high lets both go.
module k555kp15 (
    input  logic p11, input  logic p10, input  logic p9,           // S0 S1 S2
    input  logic p7,                                               // OE
    input  logic p4,  input  logic p3,  input  logic p2,  input  logic p1,   // D0..D3
    input  logic p15, input  logic p14, input  logic p13, input  logic p12,  // D4..D7
    output logic p5,  output logic p5_oe,                          // Y
    output logic p6,  output logic p6_oe                           // W
);
    logic [7:0] d;
    logic [2:0] s;

    assign d  = {p12, p13, p14, p15, p1, p2, p3, p4};
    assign s  = {p9, p10, p11};
    assign p5 = d[s];
    assign p6 = ~d[s];
    assign {p5_oe, p6_oe} = {2{~p7}};
endmodule
