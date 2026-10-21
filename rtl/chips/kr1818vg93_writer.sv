// The write channel of КР1818ВГ93 (see kr1818vg93.sv), MFM at CLK 1 MHz:
// a cell is two CLK periods (2 us), a byte 16 cells (32 us).  A 1 cell is
// a WD pulse for the CLK high half that opens it (500 ns, on CLK rising -
// docs/vg93.md §6).  A clock cell is 1 between two 0 data bits; the sync
// bytes A1 and C2 go out with their clock missing.  The byte for a cell
// pair is taken as the byte begins (`took`), so the next can be set.
module kr1818vg93_writer (
    input  logic       clk,
    input  logic       clk_pin,         // CLK, pin 24
    input  logic       run,             // WG: writing
    input  logic [7:0] byte_in,
    input  logic [1:0] kind,            // 0 data, 1 A1 sync, 2 C2 sync
    output logic       took,            // a byte begins: byte_in is taken
    output logic       wd
);
    import kr1818vg93_pkg::*;

    logic        clk_was, half, last_bit, first_period;
    logic [15:0] cells;
    logic [3:0]  cell_no;

    // the 16 cells of a data byte after a byte ending in bit p
    function automatic logic [15:0] mfm(input logic [7:0] b, input logic p);
        logic [15:0] c;
        logic        prev;
        prev = p;
        for (int i = 7; i >= 0; i--) begin
            c[2 * i + 1] = !prev && !b[i];
            c[2 * i]     = b[i];
            prev         = b[i];
        end
        return c;
    endfunction

    assign wd = run && first_period && clk_pin && cells[15];

    always_ff @(posedge clk) begin
        clk_was <= clk_pin;
        took    <= 1'b0;
        if (!run) begin
            half <= 1'b0; cell_no <= 4'd0; first_period <= 1'b0; cells <= '0;
        end else if (clk_pin && !clk_was) begin     // a CLK period begins
            half         <= !half;
            first_period <= !half;
            if (!half) begin                        // ... and a cell
                if (cell_no == 4'd0) begin
                    case (kind)
                        2'd1:    begin cells <= SYNC_A1; last_bit <= 1'b1; end
                        2'd2:    begin cells <= SYNC_C2; last_bit <= 1'b0; end
                        default: begin cells <= mfm(byte_in, last_bit); last_bit <= byte_in[0]; end
                    endcase
                    took <= 1'b1;
                end else
                    cells <= {cells[14:0], 1'b0};
                cell_no <= cell_no + 4'd1;
            end
        end
    end
endmodule
