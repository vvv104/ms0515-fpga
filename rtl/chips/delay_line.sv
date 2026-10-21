// A chip's propagation delay, inertial as a gate's is: the output rises
// once the input has been high for TICKS master clock ticks (8.33 ns each)
// and falls once it has been low for FALL ticks (FALL = TICKS by default;
// apart for an output enable, whose turn-on and turn-off differ).  A pulse
// shorter than the delay never comes out - a chip does not pass what is
// over before it could switch, which is what keeps a PROM's output clean
// while its address bits settle a tick apart.  Not a part of the board: the
// generator puts one after each output of a chip whose delay in
// netlist/delays.csv comes to a tick or more (tools/board.py).
module delay_line #(
    parameter int TICKS = 1,
    parameter int FALL  = TICKS
) (
    input  logic clk,
    input  logic i,
    output logic o
);
    localparam int N = TICKS > FALL ? TICKS : FALL;
    logic [N-1:0] stage;            // stage[k]: the input k + 1 ticks ago
    logic         held;             // the output a tick ago

    if (N == 1) begin : one
        always_ff @(posedge clk) stage <= i;
    end else begin : more
        always_ff @(posedge clk) stage <= {stage[N-2:0], i};
    end
    always_ff @(posedge clk) held <= o;

    // high all through the rise's delay, low all through the fall's
    logic high_long, low_long;
    if (TICKS == 0) begin : rise_now
        assign high_long = i;
    end else begin : rise_late
        assign high_long = &stage[TICKS-1:0];
    end
    if (FALL == 0) begin : fall_now
        assign low_long = !i;
    end else begin : fall_late
        assign low_long = ~|stage[FALL-1:0];
    end

    assign o = high_long ? 1'b1 : low_long ? 1'b0 : held;
endmodule
