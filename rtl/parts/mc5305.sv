// mc5305: the floppy drive «Электроника МС 5305» on XP4 as logic, for the
// MiSTer core (plan stage 7c) - sim/board/drive.h's drive, tick for tick:
// the spindle at 300 rpm from МВК, the index 4 ms a turn, ready from the
// third index until none has come for 450 ms, the head stepped on ШАГ's
// trailing edge, the flux a 1 us pulse at the start of each cell that
// turns.  Reading only, so far.
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
    input  logic        sd_ack,
    input  logic [8:0]  sd_buff_addr,
    input  logic [7:0]  sd_buff_dout,
    input  logic        sd_buff_wr
);
    localparam logic [6:0]  TRACK_LAST = 7'd79;
    localparam logic [7:0]  CELL_LAST = 8'd239;    // a cell: 240 ticks, 2 us at 120 MHz
    localparam logic [7:0]  PULSE = 8'd120;        // 1 us
    localparam logic [12:0] BYTES_LAST = 13'd6249;  // a turn: 6250 bytes, 100 000 cells
    localparam logic [12:0] INDEX_BYTES = 13'd125;  // 4 ms
    localparam logic [25:0] LOSS = 26'd54_000_000; // 450 ms

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
    logic       two_sides, side_n;
    logic [7:0] key, loaded;            // head and side of the track wanted, of the one loaded
    logic       have;                   // `loaded` holds a track
    logic       loading;
    logic [3:0] block;
    logic [7:0] loading_key;
    logic       ack_was;
    logic [7:0] buffer [5120];

    assign two_sides = img_size >= 64'd819200;
    assign side_n = two_sides && !side;
    assign key = {head_n, side_n};
    always_ff @(posedge clk) begin
        ack_was <= sd_ack;
        if (img_mounted) begin
            have <= 1'b0;
            loading <= 1'b0;
            sd_rd <= 1'b0;
        end else if (!loading) begin
            if (!have || loaded != key) begin
                loading <= 1'b1;
                loading_key <= key;
                block <= '0;
                have <= 1'b0;
                sd_lba <= 32'(two_sides ? key : {1'b0, key[7:1]}) * 32'd10;   // (head * sides + side) * 10
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
    end
    always_ff @(posedge clk)
        if (loading && sd_ack && sd_buff_wr)
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
        .clk, .restart, .take, .layout, .track(loaded[7:1]), .side(loaded[0]),
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
    logic in_place, pulse;
    assign in_place = synced_track && pos_n == byte_at_n;
    assign pulse = in_place && tick_at_n < PULSE && cells_n[4'd15 - cell_at_n];
    always_ff @(posedge clk) begin
        index   <= !(selected && turning_n && byte_at_n < INDEX_BYTES);
        track0  <= !(selected && head_n == 0);
        protect <= !(selected && img_readonly);
        ready   <= !(selected && indexes_n == 2'd3 && since_n < LOSS);
        data    <= !(selected && turning_n && pulse);
    end
endmodule
