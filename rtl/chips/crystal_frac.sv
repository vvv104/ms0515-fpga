// A crystal oscillator whose rating does not divide the master clock (the
// МС 7004's 4.608 MHz): a phase accumulator in kHz turns the wave on pin 1
// over whenever a half-period's worth has gone by - the frequency KHZ
// exactly on average, each edge on a master tick, a tick off at most.  At
// 4608 kHz: 24 cycles in 625 ticks, of 26 and 27 ticks.
module crystal_frac #(
    parameter int KHZ = 4608
) (
    input  logic clk,
    output logic p1
);
    localparam int MASTER_KHZ = 120_000;
    localparam int W = $clog2(MASTER_KHZ + 2 * KHZ);
    logic [W-1:0] acc;                  // from 0, as every register here

    always_ff @(posedge clk)
        if (acc + W'(2 * KHZ) >= W'(MASTER_KHZ)) begin
            acc <= acc + W'(2 * KHZ) - W'(MASTER_KHZ);
            p1 <= !p1;
        end else begin
            acc <= acc + W'(2 * KHZ);
        end
endmodule
