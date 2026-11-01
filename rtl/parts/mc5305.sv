// mc5305: the floppy drive «Электроника МС 5305» on XP4 as logic, for the
// MiSTer core (plan stage 7c) - sim/board/drive.h's drive, tick for tick
// (the flux a tick later, for timing: below):
// the spindle at 300 rpm from МВК, the index 4 ms a turn, ready from the
// third index until none has come for 450 ms, the head stepped on ШАГ's
// trailing edge, the flux a 1 us pulse at the start of each cell that
// turns; what the controller writes goes back into the image (mc5305_writer).
//
// The disk image comes through MiSTer's block interface (hps_io's sd_*,
// 512-byte blocks): the track under the head, one side's ten sectors, is
// loaded into a buffer whenever the head or the side changes.  The track's
// bytes are made as they pass the head, as the formatter `layout` wrote
// them (drive.h's omega, vvv, fdz): the gaps, the index mark, each
// sector's ID and data fields with their CRCs, Gap IV to the index; each
// byte becomes its 16 MFM cells as it begins.  After a load the bytes are
// made from the index on, faster than they pass, until they meet the head;
// till then no flux comes - inside the head's settling time, which the
// controller waits.
//
// With no disk in (no image: img_size 0) nothing is loaded or written, and
// no index, ready or flux comes; the head still moves and track 0 is seen,
// its sensor watching the head, not the disk.
//
// The lines are the bench's (drive::Lines, drive::Outputs): true is high;
// the drive's outputs are open collectors, high when let go.
module mc5305 (
    input  logic        clk,
    // the formatter that wrote the images: 0 ОМЕГА, 1 vvv, 2 FDZ
    input  logic [1:0]  layout,
    // the board's lines (Shugart, active low)
    input  logic        select0,        // ВН0 L
    input  logic        motor,          // МВК L
    input  logic        direction,      // НПШ: high out, toward track 0
    input  logic        step,           // ШАГ L
    input  logic        write_data,     // ДЗП L: each fall turns the flux
    input  logic        write_gate,     // РЗП L
    input  logic        side,           // ВПВ: high side 0
    output logic        index,          // ИНД L
    output logic        track0,         // Д00 L
    output logic        protect,        // ЗЗП L
    output logic        data,           // ДВС L
    output logic        ready,          // НГТ L
    // the image (hps_io): mounted, its size (two sides from 819 200 bytes)
    input  logic        img_mounted,
    input  logic        img_readonly,
    input  logic [63:0] img_size,
    output logic [31:0] sd_lba,
    output logic        sd_rd,
    output logic        sd_wr,
    input  logic        sd_ack,
    input  logic [8:0]  sd_buff_addr,
    input  logic [7:0]  sd_buff_dout,
    input  logic        sd_buff_wr,
    output logic [7:0]  sd_buff_din
);
    localparam logic [6:0]  TRACK_LAST = 7'd79;
    localparam logic [7:0]  CELL_LAST = 8'd239;    // a cell: 240 ticks, 2 us at 120 MHz
    localparam logic [7:0]  PULSE = 8'd120;        // 1 us
    localparam logic [12:0] BYTES_LAST = 13'd6249;  // a turn: 6250 bytes, 100 000 cells
    localparam logic [12:0] INDEX_BYTES = 13'd125;  // 4 ms
    localparam logic [25:0] LOSS = 26'd54_000_000; // 450 ms

    // the layout as the menu set it, through a register: a setting, not a
    // signal, and the layout's table is a long way to the writer's search
    logic [1:0] layout_q;
    always_ff @(posedge clk)
        layout_q <= layout;

    // ---- the spindle: where in the turn, as byte, cell and tick --------
    logic        on, turning, turning_n;
    logic [12:0] byte_at, byte_at_n;
    logic [3:0]  cell_at, cell_at_n;
    logic [7:0]  tick_at, tick_at_n;
    logic [1:0]  indexes, indexes_n;
    logic [25:0] since, since_n;
    logic        wrap;

    assign on = !motor;
    assign wrap = byte_at == BYTES_LAST && cell_at == 4'd15 && tick_at == CELL_LAST;
    always_comb begin
        turning_n = on;
        {byte_at_n, cell_at_n, tick_at_n} = {byte_at, cell_at, tick_at};
        indexes_n = indexes;
        since_n = since == LOSS ? since : since + 1'b1;
        if (on && !turning) begin                       // the first index as it starts
            {byte_at_n, cell_at_n, tick_at_n} = '0;
            indexes_n = 2'd1;
            since_n = '0;
        end else if (!on) begin                         // stopped: the turn will start at the index
            {byte_at_n, cell_at_n, tick_at_n} = '0;
        end else begin
            tick_at_n = tick_at == CELL_LAST ? 8'd0 : tick_at + 1'b1;
            if (tick_at == CELL_LAST) begin
                cell_at_n = cell_at + 1'b1;
                if (cell_at == 15)
                    byte_at_n = byte_at == BYTES_LAST ? 13'd0 : byte_at + 1'b1;
            end
            if (wrap) begin
                indexes_n = indexes == 2'd3 ? indexes : indexes + 1'b1;
                since_n = '0;
            end
        end
    end
    always_ff @(posedge clk) begin
        turning <= turning_n;
        {byte_at, cell_at, tick_at} <= {byte_at_n, cell_at_n, tick_at_n};
        indexes <= indexes_n;
        since <= since_n;
    end

    // ---- the head --------------------------------------------------------
    logic       selected, step_was;
    logic [6:0] head, head_n;
    assign selected = !select0;
    always_comb begin
        head_n = head;
        if (selected && step && !step_was && write_gate)
            head_n = direction ? (head == 7'd0 ? head : head - 1'b1)
                               : (head == TRACK_LAST ? head : head + 1'b1);
    end
    always_ff @(posedge clk) begin
        step_was <= step;
        head <= head_n;
    end

    // ---- the track under the head: loaded block by block -----------------
    logic       inserted;               // a disk in: an image mounted
    logic       two_sides, side_n;
    logic [7:0] key, loaded;            // head and side of the track wanted, of the one loaded
    logic       have;                   // `loaded` holds a track
    logic       loading;
    logic [3:0] block;
    logic [7:0] loading_key;
    logic       ack_was;
    logic [7:0] buffer [5120];

    assign inserted = img_size != 64'd0;
    assign two_sides = img_size >= 64'd819200;
    assign side_n = two_sides && !side;
    assign key = {head, side_n};        // the head as it stands: a load starts a tick after a step
    // ---- writing: the controller's fields back into sectors ---------------
    logic       writing, put, done;
    logic [8:0] put_at;
    logic [7:0] put_byte;
    logic [3:0] done_sector;
    assign writing = selected && inserted && turning_n && !write_gate && !img_readonly;
    mc5305_writer writer (
        .clk, .layout(layout_q), .writing, .write_data, .byte_now(byte_at_n),
        .put, .put_at, .put_byte, .done, .done_sector
    );

    // a field's bytes in one of two banks, the other the last field taken
    // on its way out: into the track's buffer, then the image's block
    logic [7:0] fields [1024];
    logic       fill, out_bank, pending, copying, sending;
    logic [3:0] out_sector;
    logic [9:0] copy_n;                 // the byte read; the one before written
    logic [7:0] field_q;
    logic [9:0] field_at;
    assign field_at = sending ? {out_bank, sd_buff_addr} : {out_bank, copy_n[8:0]};
    assign sd_buff_din = field_q;
    always_ff @(posedge clk)
        if (put)
            fields[{fill, put_at}] <= put_byte;
    always_ff @(posedge clk)
        field_q <= fields[field_at];

    // the track's block in the image: (head * sides + side) * 10 + block
    function automatic logic [31:0] lba(logic [7:0] k, logic two, logic [3:0] b);
        return 32'(two ? k : {1'b0, k[7:1]}) * 32'd10 + 32'(b);
    endfunction

    // the block device: a track loaded, or a field written back
    always_ff @(posedge clk) begin
        ack_was <= sd_ack;
        if (img_mounted) begin
            have <= 1'b0;
            loading <= 1'b0;
            sd_rd <= 1'b0;
            sd_wr <= 1'b0;
            pending <= 1'b0;
            copying <= 1'b0;
            sending <= 1'b0;
        end else if (copying) begin
            copy_n <= copy_n + 1'b1;
            if (copy_n == 10'd512) begin
                copying <= 1'b0;
                sending <= 1'b1;
                sd_lba <= lba(loaded, two_sides, out_sector - 1'b1);
                sd_wr <= 1'b1;
            end
        end else if (sending) begin
            if (sd_ack)
                sd_wr <= 1'b0;
            if (!sd_ack && ack_was) begin
                sending <= 1'b0;
                pending <= 1'b0;
            end
        end else if (!loading) begin
            if (pending && have) begin
                copying <= 1'b1;
                copy_n <= '0;
            end else if (inserted && (!have || loaded != key)) begin
                loading <= 1'b1;
                loading_key <= key;
                block <= '0;
                have <= 1'b0;
                sd_lba <= lba(key, two_sides, 4'd0);
                sd_rd <= 1'b1;
            end
        end else begin
            if (sd_ack)
                sd_rd <= 1'b0;
            if (!sd_ack && ack_was) begin               // a block in
                if (loading_key != key) begin           // the head moved on: start again
                    loading <= 1'b0;
                end else if (block == 4'd9) begin
                    loading <= 1'b0;
                    have <= 1'b1;
                    loaded <= loading_key;
                end else begin
                    block <= block + 1'b1;
                    sd_lba <= sd_lba + 1'b1;
                    sd_rd <= 1'b1;
                end
            end
        end
        // a field taken: its bank goes out (a field takes 16 ms to come,
        // one goes out in far less, so the bank out is free by the next)
        if (done && !img_mounted) begin
            fill <= !fill;
            out_bank <= fill;
            out_sector <= done_sector;
            pending <= 1'b1;
        end
    end
    always_ff @(posedge clk)
        if (copying && copy_n != 10'd0)
            buffer[13'(out_sector - 1'b1) * 13'd512 + 13'(copy_n - 1'b1)] <= field_q;
        else if (loading && sd_ack && sd_buff_wr)
            buffer[13'(block) * 13'd512 + 13'(sd_buff_addr)] <= sd_buff_dout;

    // ---- the track's bytes, one after another from the index -------------
    logic        restart;               // a track loaded: its bytes from the index again
    logic        take;                  // a byte wanted from the source
    logic        made;                  // ... and here
    logic [7:0]  made_byte;
    logic [1:0]  made_mark;             // 0 a plain byte, 1 A1, 2 C2 (a clock left out)
    logic [12:0] read_at;
    logic [7:0]  read_q;

    mc5305_bytes bytes (
        .clk, .restart, .take, .layout(layout_q), .track(loaded[7:1]), .side(loaded[0]),
        .read_at, .read_q, .made, .made_byte, .made_mark
    );
    always_ff @(posedge clk)
        read_q <= buffer[read_at];

    // the byte under the head as cells, the next one made; `pos` the byte
    // `cells` holds, BYTES_LAST before the first of a track
    logic [15:0] cells, cells_n;
    logic [12:0] pos, pos_n;
    logic        last;                  // the data bit before
    logic        ahead;                 // the next byte is made
    logic [7:0]  next_byte;
    logic [1:0]  next_mark;
    logic        shift;
    logic        synced_track;          // the source makes the track loaded

    function automatic logic [15:0] mfm(logic [7:0] b, logic [1:0] mark, logic last_bit);
        logic       prev;
        logic [15:0] c;
        prev = last_bit;
        for (int i = 7; i >= 0; i--) begin
            c[2 * i + 1] = !prev && !b[i] && !(mark == 2'd1 && i == 2) && !(mark == 2'd2 && i == 3);
            c[2 * i]     = b[i];
            prev = b[i];
        end
        return c;
    endfunction

    assign shift = ahead && pos != byte_at_n;
    always_comb begin
        cells_n = cells;
        pos_n = pos;
        if (shift) begin
            pos_n = pos == BYTES_LAST ? 13'd0 : pos + 1'b1;
            cells_n = mfm(next_byte, next_mark, pos == BYTES_LAST ? 1'b0 : last);
        end
    end
    assign restart = !loading && have && loaded == key && !synced_track;
    assign take = !ahead && !made && !restart && synced_track;
    always_ff @(posedge clk) begin
        cells <= cells_n;
        pos <= pos_n;
        if (shift) begin
            last <= next_byte[0];
            ahead <= 1'b0;
        end
        if (made) begin
            next_byte <= made_byte;
            next_mark <= made_mark;
            ahead <= 1'b1;
        end
        if (restart) begin
            synced_track <= 1'b1;
            ahead <= 1'b0;
            pos <= BYTES_LAST;
        end
        if (img_mounted || loading || !have || loaded != key)
            synced_track <= 1'b0;
    end

    // ---- the lines out: let go unless selected ---------------------------
    // The flux goes through one more register, a master tick (8 ns) after
    // the bench's drive: what the cell under the head is, worked out from
    // the counters' next values, takes the whole tick at 120 MHz.  The
    // pulse is 1 +-0.5 us, and the controller samples it on its own clock.
    // Its parts are taken apart a tick before they meet.
    logic in_place_was, early_was, cell_was, on_was;
    logic [12:0] pos_up;
    assign pos_up = pos == BYTES_LAST ? 13'd0 : pos + 1'b1;
    always_ff @(posedge clk) begin
        index   <= !(selected && inserted && turning_n && byte_at_n < INDEX_BYTES);
        track0  <= !(selected && head_n == 0);
        protect <= !(selected && inserted && img_readonly);
        ready   <= !(selected && inserted && indexes_n == 2'd3 && since_n < LOSS);
        // pos_n == byte_at_n, its two cases side by side: at the head
        // already, or reaching it with this shift
        in_place_was <= synced_track && (pos == byte_at_n || (ahead && pos_up == byte_at_n));
        early_was    <= tick_at_n < PULSE;
        cell_was     <= cells_n[4'd15 - cell_at_n];
        on_was       <= selected && turning_n;
        data    <= !(on_was && in_place_was && early_was && cell_was);
    end
endmodule
