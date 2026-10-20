// КР531ИД14 (74S139): two 2-to-4 decoders, outputs active low, each
// enabled by its G low.
module kr531id14 (
    input  logic p1,  input  logic p2,  input  logic p3,           // 1G 1A0 1A1
    output logic p4,  output logic p5,  output logic p6,  output logic p7,   // 1Y0..1Y3
    input  logic p15, input  logic p14, input  logic p13,          // 2G 2A0 2A1
    output logic p12, output logic p11, output logic p10, output logic p9    // 2Y0..2Y3
);
    function automatic logic [3:0] decode(input logic g, input logic [1:0] a);
        decode = '1;
        if (!g)
            decode[a] = 1'b0;
    endfunction

    assign {p7, p6, p5, p4}    = decode(p1, {p3, p2});
    assign {p9, p10, p11, p12} = decode(p15, {p13, p14});
endmodule
