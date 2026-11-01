// mc5305_pkg: the track layouts the MS 0515's formatters write (sim/board/
// drive.h: Layout, omega(), vvv(), fdz()), for the drive's logic.
package mc5305_pkg;
    localparam logic [12:0] TURN_BYTES = 13'd6250;  // a turn: 100 000 cells

    typedef struct packed {
        logic [7:0] gap1;
        logic [1:0] mark;               // the index mark: 0 none, 1 C2 C2 C2 FC, 2 C2 C2 C2 C2
        logic [7:0] index_sync, gap_after, id_sync, data_sync, gap2, gap3;
    } layout_t;

    // 0 ОМЕГА, 1 vvv, 2 FDZ
    function automatic layout_t layout_of(logic [1:0] code);
        case (code)
            2'd0:    return '{8'd54, 2'd2, 8'd12, 8'd35, 8'd8, 8'd12, 8'd22, 8'd44};
            2'd2:    return '{8'd34, 2'd0, 8'd12, 8'd0, 8'd8, 8'd12, 8'd22, 8'd45};
            default: return '{8'd80, 2'd1, 8'd12, 8'd50, 8'd12, 8'd12, 8'd22, 8'd30};
        endcase
    endfunction

    // the byte of the turn the k-th sector's data mark (FB) is in
    function automatic logic [12:0] data_mark_at(layout_t l, logic [3:0] k);
        logic [12:0] mark, id, data;
        mark = l.mark == 2'd0 ? 13'd0 : 13'(l.index_sync) + 13'd4 + 13'(l.gap_after);
        id = 13'(l.id_sync) + 13'd10;
        data = 13'(l.data_sync) + 13'd518;
        return 13'(l.gap1) + mark + 13'(k) * (id + 13'(l.gap2) + data + 13'(l.gap3)) + id + 13'(l.gap2) +
               13'(l.data_sync) + 13'd3;
    endfunction

    // CRC-CCITT a byte further
    function automatic logic [15:0] crc_byte(logic [15:0] c, logic [7:0] b);
        logic [15:0] x;
        x = c ^ {b, 8'h00};
        for (int i = 0; i < 8; i++)
            x = x[15] ? {x[14:0], 1'b0} ^ 16'h1021 : {x[14:0], 1'b0};
        return x;
    endfunction
endpackage
