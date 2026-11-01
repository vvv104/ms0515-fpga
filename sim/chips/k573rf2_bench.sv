// The test's К573РФ2: programmed with byte(a) = a[7:0] ^ a[10:8] ^ 8'hA5.
// clk: the master tick its read is registered on.
module k573rf2_bench (
    input  logic clk,
    input  logic p8,  input  logic p7,  input  logic p6,  input  logic p5,
    input  logic p4,  input  logic p3,  input  logic p2,  input  logic p1,
    input  logic p23, input  logic p22, input  logic p19,
    input  logic p18, input  logic p20,
    output logic p9,  output logic p10, output logic p11, output logic p13,
    output logic p14, output logic p15, output logic p16, output logic p17,
    output logic p9_oe,  output logic p10_oe, output logic p11_oe, output logic p13_oe,
    output logic p14_oe, output logic p15_oe, output logic p16_oe, output logic p17_oe
);
    function automatic logic [16383:0] pattern();
        logic [16383:0] c;
        for (int a = 0; a < 2048; a++)
            c[8 * a +: 8] = 8'(a) ^ 8'(a >> 8) ^ 8'hA5;
        return c;
    endfunction

    k573rf2 #(.CONTENTS(pattern())) chip (.*);
endmodule
