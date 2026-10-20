// К555ИД7 (74LS138): 3-to-8 decoder, outputs active low, enabled by
// G1 (pin 6) high and G2A, G2B (pins 4, 5) low.
module k555id7 (
    input  logic p1,  input  logic p2,  input  logic p3,           // A B C
    input  logic p4,  input  logic p5,  input  logic p6,           // G2A G2B G1
    output logic p15, output logic p14, output logic p13, output logic p12,  // Y0..Y3
    output logic p11, output logic p10, output logic p9,  output logic p7    // Y4..Y7
);
    logic       on;
    logic [2:0] a;
    logic [7:0] y;

    assign on = p6 & ~p4 & ~p5;
    assign a  = {p3, p2, p1};
    always_comb begin
        y = '1;
        if (on)
            y[a] = 1'b0;
    end
    assign {p7, p9, p10, p11, p12, p13, p14, p15} = y;
endmodule
