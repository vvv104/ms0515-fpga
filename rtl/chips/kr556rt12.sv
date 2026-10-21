// КР556РТ12: a 1024 x 4 bipolar PROM with open-collector outputs, enabled
// while both CE1 (pin 8) and CE2 (pin 10) are low; disabled, every output
// lets go.  Pins as Э3 sheet 4 draws them.  CONTENTS holds the word at
// address a in bits 4a+3..4a, bit k of a word on output Ok - a programmer's
// dump read as nibbles (firmware/README.md).
module kr556rt12 #(
    parameter logic [4095:0] CONTENTS = '0     // unprogrammed: every output low (82S136)
) (
    input  logic p5,  input  logic p6,  input  logic p7,  input  logic p4,   // A0..A3
    input  logic p3,  input  logic p2,  input  logic p1,  input  logic p17,  // A4..A7
    input  logic p16, input  logic p15,                                      // A8 A9
    input  logic p8,  input  logic p10,                                      // CE1 CE2
    output logic p14, output logic p13, output logic p12, output logic p11   // O0..O3
);
    logic [9:0] a;
    logic [3:0] word;
    assign a    = {p15, p16, p17, p1, p2, p3, p4, p7, p6, p5};
    assign word = CONTENTS[{a, 2'b00} +: 4];
    assign {p11, p12, p13, p14} = (!p8 && !p10) ? word : 4'b1111;
endmodule
