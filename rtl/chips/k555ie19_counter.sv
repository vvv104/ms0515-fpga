// One counter of К555ИЕ19 (see k555ie19.sv).
module k555ie19_counter (
    input  logic clk,
    input  logic c, input  logic r,
    output logic [3:0] q
);
    logic [3:0] count;
    logic       c_was;

    always_ff @(posedge clk) begin
        c_was <= c;
        if (r)
            count <= '0;
        else if (!c && c_was)
            count <= count + 4'd1;
    end
    assign q = r ? '0 : count;
endmodule
