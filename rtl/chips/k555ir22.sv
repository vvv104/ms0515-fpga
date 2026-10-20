// К555ИР22 (74LS373): eight transparent latches with three-state outputs;
// C (pin 11) high lets D through, low holds; OE (pin 1) low drives Q.
module k555ir22 (
    input  logic clk,
    input  logic p1,  input  logic p11,                            // OE C
    input  logic p3,  input  logic p4,  input  logic p7,  input  logic p8,   // D0..D3
    input  logic p13, input  logic p14, input  logic p17, input  logic p18,  // D4..D7
    output logic p2,  output logic p5,  output logic p6,  output logic p9,   // Q0..Q3
    output logic p12, output logic p15, output logic p16, output logic p19,  // Q4..Q7
    output logic p2_oe,  output logic p5_oe,  output logic p6_oe,  output logic p9_oe,
    output logic p12_oe, output logic p15_oe, output logic p16_oe, output logic p19_oe
);
    logic [7:0] d, held;

    assign d = {p18, p17, p14, p13, p8, p7, p4, p3};
    always_ff @(posedge clk)
        if (p11)
            held <= d;
    assign {p19, p16, p15, p12, p9, p6, p5, p2} = held;
    assign {p19_oe, p16_oe, p15_oe, p12_oe, p9_oe, p6_oe, p5_oe, p2_oe} = {8{~p1}};
endmodule
