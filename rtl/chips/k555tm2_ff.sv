// One D flip-flop of К555ТМ2 (see k555tm2.sv).
module k555tm2_ff (
    input  logic clk,
    input  logic r, input  logic d, input  logic c, input  logic s,
    output logic q, output logic qn
);
    logic state, c_was;

    always_ff @(posedge clk) begin
        c_was <= c;
        if (!s)
            state <= 1'b1;
        else if (!r)
            state <= 1'b0;
        else if (c && !c_was)
            state <= d;
    end
    assign q  = !s ? 1'b1 : (!r ? 1'b0 : state);
    assign qn = !r ? 1'b1 : (!s ? 1'b0 : ~state);
endmodule
