// One D flip-flop of К555ТМ2 (see k555tm2.sv).  Both outputs are held, so
// S and R low together give both high, as the chip does.
module k555tm2_ff (
    input  logic clk,
    input  logic r, input  logic d, input  logic c, input  logic s,
    output logic q, output logic qn
);
    logic c_was;

    always_ff @(posedge clk) begin
        c_was <= c;
        if (!s || !r) begin
            q  <= !s;
            qn <= !r;
        end else if (c && !c_was) begin
            q  <= d;
            qn <= ~d;
        end
    end
endmodule
