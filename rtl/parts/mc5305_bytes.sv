// mc5305_bytes: the bytes of a track, one after another from the index, as
// the formatter wrote them (sim/board/drive.h: Layout, track()) - for
// mc5305.sv.  A byte is asked for with `take` and comes with `made` a few
// ticks later, with its mark: 1 an A1, 2 a C2 (each written with a clock
// left out).  The data fields' bytes are read from the track's buffer
// (read_at, read_q a tick later); the CRC runs from a field's first A1.
// After the 6250 bytes of a turn the track begins again; `restart` begins
// it now.
module mc5305_bytes (
    input  logic        clk,
    input  logic        restart,
    input  logic        take,
    input  logic [1:0]  layout,         // 0 ОМЕГА, 1 vvv, 2 FDZ
    input  logic [6:0]  track,
    input  logic        side,
    output logic [12:0] read_at,
    input  logic [7:0]  read_q,
    output logic        made,
    output logic [7:0]  made_byte,
    output logic [1:0]  made_mark
);
    import mc5305_pkg::*;

    localparam logic [12:0] BYTES_LAST = TURN_BYTES - 13'd1;

    layout_t l;
    assign l = layout_of(layout);

    typedef enum logic [4:0] {
        GAP1, ISYNC, IMARK, IGAP, IDSYNC, IDA1, IDAM, ID, IDCRC, GAP2,
        DSYNC, DA1, DAM, DATA, DCRC, GAP3, GAP4
    } seg_t;
    seg_t        seg;
    logic [9:0]  n;                     // the byte of the segment
    logic [3:0]  sector;                // 0..9: sector 1..10
    logic [12:0] total;                 // the byte of the turn
    logic [15:0] crc;
    logic        busy;                  // the byte's address out, its value next

    // the segment's length
    function automatic logic [9:0] length(seg_t s);
        case (s)
            GAP1:              return 10'(l.gap1);
            ISYNC:             return 10'(l.index_sync);
            IMARK:             return 10'd4;
            IGAP:              return 10'(l.gap_after);
            IDSYNC:            return 10'(l.id_sync);
            IDA1, DA1:         return 10'd3;
            IDAM, DAM:         return 10'd1;
            ID:                return 10'd4;
            IDCRC, DCRC:       return 10'd2;
            GAP2:              return 10'(l.gap2);
            DSYNC:             return 10'(l.data_sync);
            DATA:              return 10'd512;
            GAP3:              return 10'(l.gap3);
            default:           return 10'd1;
        endcase
    endfunction

    assign read_at = 13'(sector) * 13'd512 + 13'(n);

    // this byte, its mark, and whether it goes through the CRC (from a
    // field's first A1 to its last byte before the CRC)
    logic [7:0] b;
    logic [1:0] mark;
    always_comb begin
        mark = 2'd0;
        case (seg)
            ISYNC, IDSYNC, DSYNC: b = 8'h00;
            IMARK: begin
                b = n == 3 && l.mark == 2'd1 ? 8'hFC : 8'hC2;
                mark = n == 3 && l.mark == 2'd1 ? 2'd0 : 2'd2;
            end
            IDA1, DA1: begin
                b = 8'hA1;
                mark = 2'd1;
            end
            IDAM: b = 8'hFE;
            DAM:  b = 8'hFB;
            ID:   case (n[1:0])
                      2'd0:    b = {1'b0, track};
                      2'd1:    b = {7'd0, layout != 2'd0 && side};  // ОМЕГА writes H 0 on both sides
                      2'd2:    b = 8'(sector) + 8'd1;
                      default: b = 8'd2;
                  endcase
            IDCRC, DCRC: b = n == 0 ? crc[15:8] : crc[7:0];
            DATA: b = read_q;
            default: b = 8'h4E;
        endcase
    end

    // the segment after this one
    function automatic seg_t after(seg_t s, logic [3:0] sec);
        case (s)
            GAP1:  return l.mark == 2'd0 ? IDSYNC : ISYNC;
            ISYNC: return IMARK;
            IMARK: return l.gap_after == 0 ? IDSYNC : IGAP;
            IGAP:  return IDSYNC;
            IDSYNC: return IDA1;
            IDA1:  return IDAM;
            IDAM:  return ID;
            ID:    return IDCRC;
            IDCRC: return GAP2;
            GAP2:  return DSYNC;
            DSYNC: return DA1;
            DA1:   return DAM;
            DAM:   return DATA;
            DATA:  return DCRC;
            DCRC:  return GAP3;
            GAP3:  return sec == 4'd9 ? GAP4 : IDSYNC;
            default: return GAP4;
        endcase
    endfunction

    always_ff @(posedge clk) begin
        made <= 1'b0;
        if (restart) begin
            seg <= GAP1;
            n <= '0;
            sector <= '0;
            total <= '0;
            busy <= 1'b0;
        end else if (take && !busy && !made) begin
            busy <= 1'b1;                               // the buffer is read now
        end else if (busy) begin
            busy <= 1'b0;
            made <= 1'b1;
            made_byte <= b;
            made_mark <= mark;
            if ((seg == IDA1 || seg == DA1) && n == 0)
                crc <= crc_byte(16'hFFFF, b);
            else if (seg == IDA1 || seg == DA1 || seg == IDAM || seg == DAM || seg == ID || seg == DATA)
                crc <= crc_byte(crc, b);
            if (total == BYTES_LAST) begin               // the index: the track again
                seg <= GAP1;
                n <= '0;
                sector <= '0;
                total <= '0;
            end else begin
                total <= total + 1'b1;
                if (seg != GAP4 && n == length(seg) - 1) begin
                    n <= '0;
                    seg <= after(seg, sector);
                    if (seg == GAP3)
                        sector <= sector + 1'b1;
                end else if (seg != GAP4) begin
                    n <= n + 1'b1;
                end
            end
        end
    end
endmodule
