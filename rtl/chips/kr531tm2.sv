// КР531ТМ2 (74S74): the К555ТМ2 function (Schottky, faster).
module kr531tm2 (
    input  logic clk,
    input  logic p1,  input  logic p2,  input  logic p3,  input  logic p4,
    output logic p5,  output logic p6,
    input  logic p13, input  logic p12, input  logic p11, input  logic p10,
    output logic p9,  output logic p8
);
    k555tm2 chip (.*);
endmodule
