// What the parts of the КР1818ВГ93 model share.
package kr1818vg93_pkg;
    // CRC-16 of the 179X: x^16 + x^12 + x^5 + 1, preset to ones (docs/vg93.md §3),
    // one byte in, MSB first
    function automatic logic [15:0] crc_byte(input logic [15:0] crc, input logic [7:0] b);
        logic [15:0] c;
        c = crc;
        for (int i = 7; i >= 0; i--) begin
            if (c[15] ^ b[i]) c = {c[14:0], 1'b0} ^ 16'h1021;
            else              c = {c[14:0], 1'b0};
        end
        return c;
    endfunction

    // the MFM cells of the sync bytes, their clock missing (docs/vg93.md §5)
    localparam logic [15:0] SYNC_A1 = 16'h4489;
    localparam logic [15:0] SYNC_C2 = 16'h5224;
endpackage
