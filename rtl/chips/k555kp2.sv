// К555КП2 (74LS153): two 4-to-1 multiplexers with common select S0, S1;
// an output is low while its strobe G is high.
module k555kp2 (
    input  logic p14, input  logic p2,                             // S0 S1
    input  logic p1,                                               // 1G
    input  logic p6,  input  logic p5,  input  logic p4,  input  logic p3,   // 1C0..1C3
    output logic p7,                                               // 1Y
    input  logic p15,                                              // 2G
    input  logic p10, input  logic p11, input  logic p12, input  logic p13,  // 2C0..2C3
    output logic p9                                                // 2Y
);
    logic [1:0] s;
    logic [3:0] c1, c2;

    assign s  = {p2, p14};
    assign c1 = {p3, p4, p5, p6};
    assign c2 = {p13, p12, p11, p10};
    assign p7 = ~p1 & c1[s];
    assign p9 = ~p15 & c2[s];
endmodule
