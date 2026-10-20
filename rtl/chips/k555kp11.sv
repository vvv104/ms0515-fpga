// К555КП11 (74LS257): four 2-to-1 multiplexers with three-state outputs;
// SEL (pin 1) low takes A, high takes B; OE (pin 15) high lets go.
module k555kp11 (
    input  logic p1,  input  logic p15,                            // SEL OE
    input  logic p2,  input  logic p3,  output logic p4,  output logic p4_oe,
    input  logic p5,  input  logic p6,  output logic p7,  output logic p7_oe,
    input  logic p11, input  logic p10, output logic p9,  output logic p9_oe,
    input  logic p14, input  logic p13, output logic p12, output logic p12_oe
);
    assign p4  = p1 ? p3  : p2;
    assign p7  = p1 ? p6  : p5;
    assign p9  = p1 ? p10 : p11;
    assign p12 = p1 ? p13 : p14;
    assign {p4_oe, p7_oe, p9_oe, p12_oe} = {4{~p15}};
endmodule
