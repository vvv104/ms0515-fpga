// К555ИР16 (74LS295): 4-bit shift register with parallel load, clocked on
// the falling edge of pin 9; MODE (pin 6) high loads A..D, low shifts
// right (SER into QA); three-state outputs driven while OC (pin 8) is high.
module k555ir16 (
    input  logic clk,
    input  logic p9,  input  logic p6,  input  logic p8,           // C MODE OC
    input  logic p1,                                               // SER
    input  logic p2,  input  logic p3,  input  logic p4,  input  logic p5,   // A..D
    output logic p13, output logic p12, output logic p11, output logic p10,  // QA..QD
    output logic p13_oe, output logic p12_oe, output logic p11_oe, output logic p10_oe
);
    logic [3:0] state;   // [0] = QA
    logic       c_was;

    always_ff @(posedge clk) begin
        c_was <= p9;
        if (!p9 && c_was)
            state <= p6 ? {p5, p4, p3, p2} : {state[2:0], p1};
    end
    assign {p10, p11, p12, p13} = state;
    assign {p10_oe, p11_oe, p12_oe, p13_oe} = {4{p8}};
endmodule
