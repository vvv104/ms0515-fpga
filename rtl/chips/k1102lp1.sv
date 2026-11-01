// К1102ЛП1: the МС 7004's line receiver, as the keyboard's Э3 draws its one
// use - input pin 7, output pin 2 (no datasheet at hand: netlist/ms7004/
// pinouts.csv).  It inverts, as RS-232 receivers do: the line's negative
// mark - its logic 0 here, the sense the board's line has at XS2 - comes out
// a TTL 1, so the computer's start bit is the low INT waits for (ТО 5.4).
module k1102lp1 (
    input  logic p7,                                               // A
    output logic p2                                                // Y
);
    assign p2 = ~p7;
endmodule
