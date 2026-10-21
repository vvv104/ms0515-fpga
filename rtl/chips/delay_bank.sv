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
    logic [W-1:0] stage [N];        // stage[k]: the inputs k + 1 ticks ago
    logic [W-1:0] held;             // the outputs a tick ago

    always_ff @(posedge clk) begin
        stage[0] <= i;
        for (int k = 1; k < N; k++)
            stage[k] <= stage[k - 1];
        held <= o;
    end

    // high all through the rise's delay, low all through the fall's
    logic [W-1:0] high_long, low_long;
    always_comb begin
        high_long = TICKS == 0 ? i : '1;
        for (int k = 0; k < TICKS; k++)
            high_long = high_long & stage[k];
        low_long = FALL == 0 ? ~i : '1;
        for (int k = 0; k < FALL; k++)
            low_long = low_long & ~stage[k];
    end

    assign o = high_long | (held & ~low_long);
endmodule
