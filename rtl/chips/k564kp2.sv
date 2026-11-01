// 564КП2 (CD4051B): an analogue multiplexer, eight channels X0..X7 to one
// X, the channel C B A (pins 9, 10, 11) switched through unless INH (pin 6)
// is high.  In logic levels the channel switched through passes from its X
// to the common X; with none, X reads low (on the МС 7004 a resistor holds
// each channel low and the amplifier after it reads nothing).
module k564kp2 (
    input  logic p13, input  logic p14, input  logic p15, input  logic p12,  // X0..X3
    input  logic p1,  input  logic p5,  input  logic p2,  input  logic p4,   // X4..X7
    input  logic p11, input  logic p10, input  logic p9,                     // A B C
    input  logic p6,                                                         // INH
    output logic p3                                                          // X
);
    logic [7:0] x;
    assign x = {p4, p2, p5, p1, p12, p15, p14, p13};
    assign p3 = !p6 && x[{p9, p10, p11}];
endmodule
