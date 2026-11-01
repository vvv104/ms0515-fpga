// A crystal oscillator whose rating does not divide the master clock (the
// МС 7004's 4.608 MHz): a phase accumulator turns the wave on pin 1 over
// ADD times in every MOD master ticks - the rating exactly on average, each
// edge on a master tick, a tick off at most.  ADD / MOD is twice the rating
// over the master clock, reduced (tools/board.py works it out): at 4608 kHz
// 48 / 625, 24 cycles in 625 ticks, of 26 and 27 ticks.
module crystal_frac #(
    parameter int ADD = 48,
    parameter int MOD = 625
) (
    input  logic clk,
    output logic p1
);
    localparam int W = $clog2(MOD + ADD);
    logic [W-1:0] acc;                  // from 0, as every register here

    always_ff @(posedge clk)
        if (acc + W'(ADD) >= W'(MOD)) begin
            acc <= acc + W'(ADD) - W'(MOD);
            p1 <= !p1;
        end else begin
            acc <= acc + W'(ADD);
        end
endmodule
