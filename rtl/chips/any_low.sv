// A resistor pack summing lines into its common pin, read as logic: the
// МС 7007's detector DA2 (НР1-11А, 10 kOhm each) with R20 to +5 V on the
// common.  The lines rest high through the pack; a key pulls one low
// through its diode and drags the common below the R18/R19 divider that
// D48's receiver compares it with (docs/analog.md).  So the common is low
// when any line is.
module any_low #(
    parameter int N = 1
) (
    input  logic [N-1:0] i,
    output logic o
);
    assign o = &i;
endmodule
