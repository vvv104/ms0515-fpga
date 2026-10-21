// КР1102АП15 (uA9636): two inverting RS-232 line drivers - 1A (pin 2) to
// 1Y (pin 7), 2A (pin 3) to 2Y (pin 6), each inverted: a TTL low puts out
// the line's positive level (eandc К1102АП15 p.1; TI uA9636AC SLLS110B p.1,
// p.3).  Pin 1 (W-S) takes the resistor that sets the outputs' slew rate -
// on the board 10 kOhm to ground (R6, R12) - and has no logic: it is not a
// port.  On the board the С2 interface's TXD drives 1A and its DTR 2A.
module kr1102ap15 (
    input  logic p2,  output logic p7,                             // 1A 1Y
    input  logic p3,  output logic p6                              // 2A 2Y
);
    assign p7 = ~p2;
    assign p6 = ~p3;
endmodule
