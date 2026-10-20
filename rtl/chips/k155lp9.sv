// К155ЛП9 (7407): six buffers, open-collector high-voltage outputs.
module k155lp9 (
    input  logic p1, output logic p2,
    input  logic p3, output logic p4,
    input  logic p5, output logic p6,
    input  logic p9, output logic p8,
    input  logic p11, output logic p10,
    input  logic p13, output logic p12
);
    assign p2 = p1;
    assign p4 = p3;
    assign p6 = p5;
    assign p8 = p9;
    assign p10 = p11;
    assign p12 = p13;
endmodule
