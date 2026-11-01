// mc5305_writer: what the controller writes, back into sectors - for
// mc5305.sv, as sim/board/drive.h's Writer does it.  While `writing` (РЗП
// on, the drive selected and turning, the disk not protected) the falls of
// ДЗП L become cells: between two falls as many 2 us cells as fit, rounded,
// the first fall a cell of its own - the writer's clock and the turn's need
// not agree.  The cells are read as the controller reads them: a field from
// its A1s (the missing clock, 4489), its mark, an ID (FE) naming the sector
// of the data field (FB or F8) after it.  A data field's bytes go out as
// they come (put); with its CRC right, `done` gives its sector - the ID's,
// or with none read in this write the one whose data field the layout puts
// nearest to where it was written.
module mc5305_writer (
    input  logic        clk,
    input  logic [1:0]  layout,
    input  logic        writing,
    input  logic        write_data,     // ДЗП L
    input  logic [12:0] byte_now,       // the byte of the turn under the head
    output logic        put,
    output logic [8:0]  put_at,
    output logic [7:0]  put_byte,
    output logic        done,
    output logic [3:0]  done_sector
);
    import mc5305_pkg::*;

    localparam logic [7:0]  CELL_LAST = 8'd239;
    localparam logic [7:0]  HALF = 8'd120;
    localparam logic [15:0] CRC_A1S = 16'hCDB4;     // FFFF through A1 A1 A1

    typedef enum logic [1:0] {HUNT, MARK, ID, DATA} state_t;

    // the decoder's state, and one cell through it
    typedef struct packed {
        state_t      st;
        logic [15:0] window;
        logic [3:0]  got;               // cells of the byte
        logic [17:0] cells;             // since the first fall
    } dec_t;

    function automatic dec_t one_cell(dec_t d, logic c);
        dec_t o = d;
        o.window = {d.window[14:0], c};
        o.cells = d.cells + 1'b1;
        if (d.st != ID && d.st != DATA && o.window == 16'h4489) begin
            o.st = MARK;                                // an A1: the bytes from here
            o.got = '0;
        end else if (d.st != HUNT) begin
            o.got = d.got + 1'b1;
        end
        return o;
    endfunction

    // a byte is done as the 16th cell after a sync comes in, unless that
    // cell makes another A1
    function automatic logic done_in(state_t st_was, logic [3:0] got_was, logic [15:0] window_now);
        return st_was != HUNT && got_was == 4'd15 && !(window_now == 16'h4489 && st_was == MARK);
    endfunction

    // a byte count from the index, into the turn (a write is shorter than
    // three turns)
    function automatic logic [12:0] turn_byte(logic [14:0] v);
        logic [14:0] x = v;
        for (int i = 0; i < 3; i++)
            if (x >= 15'(TURN_BYTES)) x = x - 15'(TURN_BYTES);
        return x[12:0];
    endfunction

    // the cells: since the last fall, in ticks and in cells gone by (0, 1, 2 or more)
    dec_t       dec;
    logic       writing_was, data_was, first;
    logic [7:0] acc;
    logic [1:0] gone;
    logic       fresh, first_now, fall, wrap, zero;
    assign fresh = writing && !writing_was;
    assign first_now = fresh || first;
    assign fall = writing && data_was && !write_data;
    assign wrap = writing && !first_now && acc == CELL_LAST;
    assign zero = wrap && gone != 2'd0;                 // a whole cell gone by with no turn

    // this tick's cells (a cell gone by and a fall can share a tick)
    dec_t       d0, d1, d2;
    logic       byte_done;
    logic [7:0] b;
    logic [17:0] byte_cells;
    always_comb begin
        d0 = fresh ? '0 : dec;                         // a write begins: HUNT, nothing seen
        d1 = zero ? one_cell(d0, 1'b0) : d0;
        d2 = fall ? one_cell(d1, 1'b1) : d1;
        byte_done = 1'b0;
        b = 8'd0;
        byte_cells = d2.cells;
        // a byte's data cells: the second of each pair
        if (zero && done_in(d0.st, d0.got, d1.window)) begin
            byte_done = 1'b1;
            b = {d1.window[14], d1.window[12], d1.window[10], d1.window[8],
                 d1.window[6], d1.window[4], d1.window[2], d1.window[0]};
            byte_cells = d1.cells;
        end else if (fall && done_in(d1.st, d1.got, d2.window)) begin
            byte_done = 1'b1;
            b = {d2.window[14], d2.window[12], d2.window[10], d2.window[8],
                 d2.window[6], d2.window[4], d2.window[2], d2.window[0]};
        end
    end

    // the fields

    logic [15:0] crc, crc_n;
    logic [9:0]  n;
    logic [7:0]  r, sector_id;          // the ID's R; the sector the last good ID named (0 none)
    logic [12:0] first_byte, mark_byte;
    logic [3:0]  nearest;
    assign crc_n = crc_byte(crc, b);

    always_ff @(posedge clk) begin
        writing_was <= writing;
        data_was <= write_data;
        put <= 1'b0;
        done <= 1'b0;
        if (fresh) begin
            first <= 1'b1;
            sector_id <= '0;
        end
        if (writing) begin
            dec <= d2;
            if (fall) begin
                if (first_now) first_byte <= byte_now;
                first <= 1'b0;
                acc <= HALF;
                gone <= '0;
            end else if (!first_now) begin
                acc <= wrap ? 8'd0 : acc + 1'b1;
                if (wrap && gone != 2'd2) gone <= gone + 1'b1;
            end
            if (byte_done)
                case (d0.st)
                    MARK: begin
                        n <= '0;
                        crc <= crc_byte(CRC_A1S, b);
                        mark_byte <= turn_byte(15'(first_byte) + 15'((byte_cells - 18'd16) >> 4));
                        dec.st <= b == 8'hFE ? ID : (b == 8'hFB || b == 8'hF8) ? DATA : HUNT;
                    end
                    ID: begin
                        n <= n + 1'b1;
                        crc <= crc_n;
                        if (n == 10'd2) r <= b;
                        if (n == 10'd5) begin
                            if (crc_n == 16'd0) sector_id <= r;
                            dec.st <= HUNT;
                        end
                    end
                    DATA: begin
                        n <= n + 1'b1;
                        crc <= crc_n;
                        if (n < 10'd512) begin
                            put <= 1'b1;
                            put_at <= n[8:0];
                            put_byte <= b;
                        end
                        if (n == 10'd513) begin
                            if (crc_n == 16'd0) begin
                                done <= 1'b1;
                                done_sector <= sector_id >= 8'd1 && sector_id <= 8'd10 ? sector_id[3:0]
                                                                                       : nearest;
                                sector_id <= '0;
                            end
                            dec.st <= HUNT;
                        end
                    end
                    default: ;
                endcase
        end
    end

    // the sector whose data mark the layout puts nearest to mark_byte,
    // sought a sector a tick after the mark (the field's 514 bytes give time)
    layout_t     l;
    logic [3:0]  seek;                  // the sector looked at, 1..10; 0 done
    logic [12:0] there, step, best_d, d;
    assign l = layout_of(layout);
    assign step = 13'(l.id_sync) + 13'd10 + 13'(l.gap2) + 13'(l.data_sync) + 13'd518 + 13'(l.gap3);
    always_comb begin
        d = there >= mark_byte ? there - mark_byte : mark_byte - there;
        if (TURN_BYTES - d < d) d = TURN_BYTES - d;
    end
    always_ff @(posedge clk)
        if (writing && byte_done && d0.st == MARK) begin
            seek <= 4'd1;
            there <= data_mark_at(l, 4'd0);
            best_d <= TURN_BYTES;
        end else if (seek != 4'd0) begin
            if (d < best_d) begin
                best_d <= d;
                nearest <= seek;
            end
            there <= there + step;
            seek <= seek == 4'd10 ? 4'd0 : seek + 1'b1;
        end
endmodule
