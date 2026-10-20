// К555ЛР11 (74LS51): two AND-OR-INVERT gates,
// 1Y = not(1A.1B.1C + 1D.1E.1F), 2Y = not(2A.2B + 2C.2D).
module k555lr11 (
    input  logic p1,  input  logic p12, input  logic p13,
    input  logic p9,  input  logic p10, input  logic p11, output logic p8,
    input  logic p2,  input  logic p3,
    input  logic p4,  input  logic p5,  output logic p6
);
    assign p8 = ~((p1 & p12 & p13) | (p9 & p10 & p11));
    assign p6 = ~((p2 & p3) | (p4 & p5));
endmodule
