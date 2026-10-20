// К555ЛИ3 (74LS11): three 3-input AND gates.
module k555li3 (
    input  logic p1, input  logic p2, input  logic p13, output logic p12,
    input  logic p3, input  logic p4, input  logic p5, output logic p6,
    input  logic p9, input  logic p10, input  logic p11, output logic p8
);
    assign p12 = p1 & p2 & p13;
    assign p6 = p3 & p4 & p5;
    assign p8 = p9 & p10 & p11;
endmodule
