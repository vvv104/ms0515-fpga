// К555ИЕ19 (74LS393): two 4-bit binary counters, each counting on the
// falling edge of its clock and cleared while its R is high.  The chip
// ripples inside; here a counter's bits change together on its edge.
module k555ie19 (
    input  logic clk,
    input  logic p1,  input  logic p2,                             // 1C 1R
    output logic p3,  output logic p4,  output logic p5,  output logic p6,   // 1Q1..1Q8
    input  logic p13, input  logic p12,                            // 2C 2R
    output logic p11, output logic p10, output logic p9,  output logic p8    // 2Q1..2Q8
);
    k555ie19_counter c1 (.clk, .c(p1),  .r(p2),  .q({p6, p5, p4, p3}));
    k555ie19_counter c2 (.clk, .c(p13), .r(p12), .q({p8, p9, p10, p11}));
endmodule
