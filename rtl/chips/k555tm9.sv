// К555ТМ9 (74LS174): six D flip-flops, common rising clock (pin 9),
// asynchronous clear (pin 1, active low).
module k555tm9 (
    input  logic clk,
    input  logic p1,  input  logic p9,                             // CLR C
    input  logic p3,  input  logic p4,  input  logic p6,           // D0 D1 D2
    input  logic p11, input  logic p13, input  logic p14,          // D3 D4 D5
    output logic p2,  output logic p5,  output logic p7,           // Q0 Q1 Q2
    output logic p10, output logic p12, output logic p15           // Q3 Q4 Q5
);
    logic [5:0] state;
    logic       c_was;

    always_ff @(posedge clk) begin
        c_was <= p9;
        if (!p1)
            state <= '0;
        else if (p9 && !c_was)
            state <= {p14, p13, p11, p6, p4, p3};
    end
    assign {p15, p12, p10, p7, p5, p2} = state;
endmodule
