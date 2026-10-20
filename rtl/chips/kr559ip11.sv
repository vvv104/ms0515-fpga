// КР559ИП11 (AM26LS32): four differential line receivers, three-state,
// enabled while EN (pin 4) is high or ENN (pin 12) is low.  In logic levels
// an input is 1 when it is above the other input of its pair: Y is 1 while
// A is above B, and also with both inputs alike (the receiver's fail-safe
// high for an open line, which is what a level of 0 on both stands for).
module kr559ip11 (
    input  logic p4,  input  logic p12,                                         // EN ENN
    input  logic p2,  input  logic p1,  output logic p3,  output logic p3_oe,    // 1A 1B 1Y
    input  logic p6,  input  logic p7,  output logic p5,  output logic p5_oe,    // 2A 2B 2Y
    input  logic p10, input  logic p9,  output logic p11, output logic p11_oe,   // 3A 3B 3Y
    input  logic p14, input  logic p15, output logic p13, output logic p13_oe    // 4A 4B 4Y
);
    logic on;
    assign on  = p4 | ~p12;
    assign p3  = p2  | ~p1;
    assign p5  = p6  | ~p7;
    assign p11 = p10 | ~p9;
    assign p13 = p14 | ~p15;
    assign {p3_oe, p5_oe, p11_oe, p13_oe} = {4{on}};
endmodule
