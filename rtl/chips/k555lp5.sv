// К555ЛП5 (74LS86): four 2-input exclusive-OR gates.
module k555lp5 (
    input  logic p1, input  logic p2, output logic p3,
    input  logic p4, input  logic p5, output logic p6,
    input  logic p9, input  logic p10, output logic p8,
    input  logic p12, input  logic p13, output logic p11
);
    assign p3 = p1 ^ p2;
    assign p6 = p4 ^ p5;
    assign p8 = p9 ^ p10;
    assign p11 = p12 ^ p13;
endmodule
