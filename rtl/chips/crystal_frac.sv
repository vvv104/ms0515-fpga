// A crystal oscillator whose rating does not divide the master clock (the
// МС 7004's 4.608 MHz): a phase accumulator turns the wave on pin 1 over
// whenever a half-period's worth has gone by - the frequency KHZ exactly on
// average, each edge on a master tick, a tick off at most.  At 4608 kHz:
// 24 cycles in 625 ticks, of 26 and 27 ticks.  The step and the modulus are
// 2 * KHZ and the master clock in kHz, both divided by their greatest
// common divisor (no bit of the accumulator that never changes).
module crystal_frac #(
    parameter int KHZ = 4608
) (
    input  logic clk,
    output logic p1
);
    function automatic int gcd(int a, int b);
        while (b != 0) begin
            int r = a % b;
            a = b;
            b = r;
        end
        return a;
    endfunction

    localparam int G = gcd(2 * KHZ, 120_000);
    localparam int ADD = 2 * KHZ / G;
    localparam int MOD = 120_000 / G;
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
