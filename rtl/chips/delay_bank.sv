// Chips' propagation delay, inertial as a gate's is, for W outputs at once:
// each bit rises once its input has been high for TICKS master clock ticks
// (8.33 ns each) and falls once it has been low for FALL ticks (FALL = TICKS
// by default; apart for an output enable, whose turn-on and turn-off
// differ).  A pulse shorter than the delay never comes out - a chip does
// not pass what is over before it could switch, which is what keeps a
// PROM's output clean while its address bits settle a tick apart.  The
// bits are independent; one bank serves every output with the same delays,
// a machine word's worth at a time.  Not a part of the board: the
// generator puts the chips' outputs through them, by netlist/delays.csv
// (tools/board.py).
//
// Whether a level has held for the whole delay is worked out a tick ahead,
// from the input and the ticks before it, and kept in a register: the
// output is then a gate away from flip-flops.  (An AND of eighteen stages
// after them left the FPGA's tick no time for the bus the output drives.)
module delay_bank #(
    parameter int TICKS = 1,
    parameter int FALL  = TICKS,
    parameter int W     = 1
) (
    input  logic         clk,
    input  logic [W-1:0] i,
    output logic [W-1:0] o
);
    localparam int N = TICKS > FALL ? TICKS : FALL;
    logic [W-1:0] held;             // the outputs a tick ago
    logic [W-1:0] high_long;        // high all through the rise's delay
    logic [W-1:0] low_long;         // low all through the fall's

    always_ff @(posedge clk)
        held <= o;

    generate if (N > 1) begin : history
        logic [W-1:0] seen [N - 1];    // seen[k]: the inputs k + 1 ticks ago
        always_ff @(posedge clk) begin
            seen[0] <= i;
            for (int k = 1; k < N - 1; k++)
                seen[k] <= seen[k - 1];
        end
        if (TICKS > 0) begin : rise
            logic [W-1:0] next;
            always_comb begin
                next = i;
                for (int k = 0; k < TICKS - 1; k++)
                    next = next & seen[k];
            end
            always_ff @(posedge clk)
                high_long <= next;
        end else begin : rise_now
            assign high_long = i;
        end
        if (FALL > 0) begin : fall
            logic [W-1:0] next;
            always_comb begin
                next = ~i;
                for (int k = 0; k < FALL - 1; k++)
                    next = next & ~seen[k];
            end
            always_ff @(posedge clk)
                low_long <= next;
        end else begin : fall_now
            assign low_long = ~i;
        end
    end else begin : at_most_a_tick
        if (TICKS > 0) begin : rise
            always_ff @(posedge clk)
                high_long <= i;
        end else begin : rise_now
            assign high_long = i;
        end
        if (FALL > 0) begin : fall
            always_ff @(posedge clk)
                low_long <= ~i;
        end else begin : fall_now
            assign low_long = ~i;
        end
    end endgenerate

    assign o = high_long | (held & ~low_long);
endmodule
