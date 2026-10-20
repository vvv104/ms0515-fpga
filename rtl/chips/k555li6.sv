// К555ЛИ6 (74LS21): two 4-input AND gates.
module k555li6 (
    input  logic p1, input  logic p2, input  logic p4, input  logic p5, output logic p6,
    input  logic p9, input  logic p10, input  logic p12, input  logic p13, output logic p8
);
    assign p6 = p1 & p2 & p4 & p5;
    assign p8 = p9 & p10 & p12 & p13;
endmodule
