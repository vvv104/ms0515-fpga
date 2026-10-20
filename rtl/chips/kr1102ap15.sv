// КР1102АП15: two inverting line drivers for the С2 (RS-232) interface,
// gated by a common strobe E.  No datasheet is at hand; the function is the
// one the board uses it for: the УСАПП's TXD (B) comes out on Y and its DTR
// (A) on YN, each inverted - a 1 out is the line's positive level, the
// interface's logical 0 - and E lets them through.  On the board E has
// only 10 kOhm to ground (R6, R12), which a TTL input still reads high.
module kr1102ap15 (
    input  logic p1,                                               // E
    input  logic p2,  output logic p7,                             // B Y
    input  logic p3,  output logic p6                              // A YN
);
    assign p7 = ~(p1 & p2);
    assign p6 = ~(p1 & p3);
endmodule
