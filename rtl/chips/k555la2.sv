// К555ЛА2 (74LS30): an 8-input NAND gate.
module k555la2 (
    input  logic p1, input  logic p2, input  logic p3, input  logic p4, input  logic p5, input  logic p6, input  logic p11, input  logic p12, output logic p8
);
    assign p8 = ~(p1 & p2 & p3 & p4 & p5 & p6 & p11 & p12);
endmodule
