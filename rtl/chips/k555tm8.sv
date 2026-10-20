// К555ТМ8 (74LS175): four D flip-flops with Q and not-Q, common rising
// clock (pin 9), asynchronous clear (pin 1, active low).
module k555tm8 (
    input  logic clk,
    input  logic p1,  input  logic p9,                             // CLR C
    input  logic p4,  input  logic p5,  input  logic p12, input  logic p13,  // 1D..4D
    output logic p2,  output logic p7,  output logic p10, output logic p15,  // 1Q..4Q
    output logic p3,  output logic p6,  output logic p11, output logic p14   // 1QN..4QN
);
    logic [3:0] state, q;
    logic       c_was;

    always_ff @(posedge clk) begin
        c_was <= p9;
        if (!p1)
            state <= '0;
        else if (p9 && !c_was)
            state <= {p13, p12, p5, p4};
    end
    assign q = state;
    assign {p15, p10, p7, p2}  = q;
    assign {p14, p11, p6, p3}  = ~q;
endmodule
