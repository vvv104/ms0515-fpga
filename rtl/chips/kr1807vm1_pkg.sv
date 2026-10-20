// What the parts of the КР1807ВМ1 model share.
package kr1807vm1_pkg;
    // bus transactions (docs/t11-model.md)
    localparam logic [2:0] BUS_NOP   = 3'd0;   // 3T, no strobes
    localparam logic [2:0] BUS_READ  = 3'd1;   // 6T
    localparam logic [2:0] BUS_WRITE = 3'd2;   // 6T
    localparam logic [2:0] BUS_ASPI  = 3'd3;   // 4T: -CAS and PI, requests sampled
    localparam logic [2:0] BUS_IACK  = 3'd4;   // 4T: SEL1, -RAS, vector read
    localparam logic [2:0] BUS_DMG   = 3'd5;   // 8T: a DMA grant, SEL 11, DAL let go
endpackage
