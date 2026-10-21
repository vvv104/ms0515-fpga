// The test's К573РФ4: programmed with byte(a) = a[7:0] ^ a[12:8] ^ 8'h5A.
module k573rf4_bench (
    input  logic p10, input  logic p9,  input  logic p8,  input  logic p7,
    input  logic p6,  input  logic p5,  input  logic p4,  input  logic p3,
    input  logic p25, input  logic p24, input  logic p21, input  logic p23,
    input  logic p2,
    input  logic p20, input  logic p22, input  logic p27,
    output logic p11, output logic p12, output logic p13, output logic p15,
    output logic p16, output logic p17, output logic p18, output logic p19,
    output logic p11_oe, output logic p12_oe, output logic p13_oe, output logic p15_oe,
    output logic p16_oe, output logic p17_oe, output logic p18_oe, output logic p19_oe
);
    function automatic logic [65535:0] pattern();
        logic [65535:0] c;
        for (int a = 0; a < 8192; a++)
            c[8 * a +: 8] = 8'(a) ^ 8'(a >> 8) ^ 8'h5A;
        return c;
    endfunction

    k573rf4 #(.CONTENTS(pattern())) chip (.*);
endmodule
