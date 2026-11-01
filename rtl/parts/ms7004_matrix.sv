// ms7004_matrix: the МС 7004's matrix board as logic, for the MiSTer core -
// a pressed key (ms7004_keys.sv) puts the pulse of its scan line ВРk on its
// return line ГРm, as its capacitive sensor does (ТО 5.2; the keys' places:
// netlist/ms7004/matrix.csv).  A key's place is (m - 1) * 16 + (k - 1); the
// lines are bit k - 1 and m - 1.
module ms7004_matrix (
    input  logic [127:0] pressed,
    input  logic [15:0]  vr,
    output logic [7:0]   gr
);
    always_comb
        for (int m = 0; m < 8; m++)
            gr[m] = |(pressed[16 * m +: 16] & vr);
endmodule
