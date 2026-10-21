// The test's КР556РТ12: programmed with a pattern whose every word differs
// from its neighbours', word(a) = a[3:0] ^ a[7:4] ^ {a[9:8], 2'b00}.
module kr556rt12_bench (
    input  logic p5,  input  logic p6,  input  logic p7,  input  logic p4,
    input  logic p3,  input  logic p2,  input  logic p1,  input  logic p17,
    input  logic p16, input  logic p15,
    input  logic p8,  input  logic p10,
    output logic p14, output logic p13, output logic p12, output logic p11
);
    function automatic logic [4095:0] pattern();
        logic [4095:0] c;
        for (int a = 0; a < 1024; a++)
            c[4 * a +: 4] = 4'(a) ^ 4'(a >> 4) ^ {2'(a >> 8), 2'b00};
        return c;
    endfunction

    kr556rt12 #(.CONTENTS(pattern())) chip (.*);
endmodule
