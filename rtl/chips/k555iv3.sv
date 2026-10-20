// К555ИВ3 (74LS147): 10-to-4 priority encoder; inputs I1..I9 active low,
// the highest active one wins, the output A3..A0 is its number inverted.
module k555iv3 (
    input  logic p11, input  logic p12, input  logic p13,          // I1 I2 I3
    input  logic p1,  input  logic p2,  input  logic p3,           // I4 I5 I6
    input  logic p4,  input  logic p5,  input  logic p10,          // I7 I8 I9
    output logic p9,  output logic p7,  output logic p6,  output logic p14   // A0..A3
);
    logic [9:1] i;
    logic [3:0] n;

    assign i = ~{p10, p5, p4, p3, p2, p1, p13, p12, p11};
    always_comb begin
        n = 4'd0;
        for (int k = 1; k <= 9; k++)
            if (i[k])
                n = 4'(k);
    end
    assign {p14, p6, p7, p9} = ~n;
endmodule
