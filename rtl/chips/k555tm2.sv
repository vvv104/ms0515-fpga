// К555ТМ2 (74LS74): two D flip-flops, rising edge, asynchronous set S and
// reset R (active low); both low drive both outputs high.
module k555tm2 (
    input  logic clk,
    input  logic p1,  input  logic p2,  input  logic p3,  input  logic p4,   // 1R 1D 1C 1S
    output logic p5,  output logic p6,                                       // 1Q 1QN
    input  logic p13, input  logic p12, input  logic p11, input  logic p10,  // 2R 2D 2C 2S
    output logic p9,  output logic p8                                        // 2Q 2QN
);
    k555tm2_ff ff1 (.clk, .r(p1),  .d(p2),  .c(p3),  .s(p4),  .q(p5), .qn(p6));
    k555tm2_ff ff2 (.clk, .r(p13), .d(p12), .c(p11), .s(p10), .q(p9), .qn(p8));
endmodule
