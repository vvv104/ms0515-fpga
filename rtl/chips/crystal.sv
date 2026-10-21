// A crystal with its oscillator: the resonator and the gates around it
// (D15 gates 6 and 5 around BQ1, D91 gates 1 and 2 around BQ2) settle into
// a square wave at its rating; here the wave is made of the master clock
// and put on pin 1, the side facing the first gate's input, where the gates
// take it on.  Pin 2 faces the second gate's output: nothing to drive.
// PERIOD master ticks a cycle, high for the first HIGH of them (the
// generator works both out of the rating, tools/board.py).
module crystal #(
    parameter int PERIOD = 15,
    parameter int HIGH   = 8
) (
    input  logic clk,
    output logic p1
);
    localparam int W = $clog2(PERIOD);
    logic [W-1:0] phase;                // from 0, as every register here

    always_ff @(posedge clk)
        phase <= phase == W'(PERIOD - 1) ? '0 : phase + W'(1);

    assign p1 = phase < W'(HIGH);
endmodule
