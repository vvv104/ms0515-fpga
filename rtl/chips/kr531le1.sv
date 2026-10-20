// КР531ЛЕ1 (74S02): four 2-input NOR gates.
module kr531le1 (
    input  logic p2, input  logic p3, output logic p1,
    input  logic p5, input  logic p6, output logic p4,
    input  logic p8, input  logic p9, output logic p10,
    input  logic p11, input  logic p12, output logic p13
);
    assign p1 = ~(p2 | p3);
    assign p4 = ~(p5 | p6);
    assign p10 = ~(p8 | p9);
    assign p13 = ~(p11 | p12);
endmodule
