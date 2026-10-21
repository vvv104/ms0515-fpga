// The read channel of КР1818ВГ93 (see kr1818vg93.sv), MFM: each half of
// RCLK is a window, one cell; a RAW READ pulse inside a window makes the
// cell 1.  The cells run through a 16-cell register; the sync pattern of
// A1 with its clock missing (4489) frames the bytes, and from there every
// 16 cells are a byte, its bits the second cell of each pair.  The pattern
// is looked for at every cell, so each A1 frames anew.  Which RCLK half
// is the data window the datasheet does not say: the sync decides it.
module kr1818vg93_reader (
    input  logic       clk,
    input  logic       rclk,            // RCLK, pin 26
    input  logic       raw_n,           // -RAW READ, pin 27
    output logic       byte_valid,      // a byte framed
    output logic [7:0] byte_value,
    output logic       sync             // ... it is an A1 found by its pattern
);
    import kr1818vg93_pkg::*;

    logic        rclk_was, raw_was, seen, framed, window;
    logic [14:0] cells;
    logic [15:0] shifted;
    logic [3:0]  count;

    assign window = seen || (raw_was && !raw_n);      // a pulse in the window that ends
    assign shifted = {cells, window};


    always_ff @(posedge clk) begin
        rclk_was   <= rclk;
        raw_was    <= raw_n;
        byte_valid <= 1'b0;
        if (raw_was && !raw_n)
            seen <= 1'b1;
        if (rclk != rclk_was) begin                 // a window ends
            cells <= shifted[14:0];
            seen  <= 1'b0;
            if (shifted == SYNC_A1) begin
                framed     <= 1'b1;
                count      <= 4'd0;
                byte_valid <= 1'b1;
                byte_value <= 8'hA1;
                sync       <= 1'b1;
            end else if (framed) begin
                count <= count + 4'd1;
                if (count == 4'd15) begin
                    byte_valid <= 1'b1;
                    // the data bits, oldest first: cells 14, 12, .. 0
                    byte_value <= {shifted[14], shifted[12], shifted[10], shifted[8],
                                   shifted[6], shifted[4], shifted[2], shifted[0]};
                    sync       <= 1'b0;
                end
            end
        end
    end
endmodule
