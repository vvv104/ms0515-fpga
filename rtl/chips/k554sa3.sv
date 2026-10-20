// К554СА3А (LM311): voltage comparator with an open-collector output.  In
// logic levels IN+ at 1 stands for IN+ above IN-: the output lets go (1)
// while IN+ is above IN- and pulls low otherwise.
module k554sa3 (
    input  logic p3,  input  logic p4,                             // IN+ IN-
    output logic p9                                                // OUT
);
    assign p9 = p3 & ~p4;
endmodule
