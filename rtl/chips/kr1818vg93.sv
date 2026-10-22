// КР1818ВГ93 (WD FD1793) at its pins, by the FD179X-02 datasheet
// (docs/vg93.md): the registers, the four command types with their times
// at CLK 1 MHz, DRQ and INTRQ, the MFM read and write channels
// (kr1818vg93_reader, kr1818vg93_writer).  DAL is true, as on the 1793.
// FM (DDEN high) is not modelled: НС4 ties DDEN low; with DDEN high the
// read channel finds nothing.  DRQ and INTRQ drive both levels - НС4 puts
// no pull-up on them - and the model's other choices close docs/vg93.md.
module kr1818vg93 (
    input  logic clk,
    // DAL0..DAL7 (pins 7-14)
    input  logic p7_in, p8_in, p9_in, p10_in, p11_in, p12_in, p13_in, p14_in,
    output logic p7, p8, p9, p10, p11, p12, p13, p14,
    output logic p7_oe, p8_oe, p9_oe, p10_oe, p11_oe, p12_oe, p13_oe, p14_oe,
    input  logic p2, p3, p4, p5, p6,            // -WE -CS -RE A0 A1
    input  logic p19, p22, p23, p24,            // -MR -TEST HLT CLK
    input  logic p26, p27,                      // RCLK -RAW READ
    input  logic p32, p34, p35, p36, p37,       // READY -TR00 -IP -WPRT -DDEN
    input  logic p33_in,                        // -WF (while WG)
    output logic p33, p33_oe,                   // -VFOE (while not WG)
    output logic p15, p16, p17, p18,            // STEP DIRC EARLY LATE
    output logic p25, p28, p29, p30, p31,       // RG HLD TG43 WG WD
    output logic p38, p39                       // DRQ INTRQ
);
    import kr1818vg93_pkg::*;

    // ── the bus ──────────────────────────────────────────────────────────
    logic [7:0] d_in, d_was, cr, tr, sr, dr, status, bus;
    logic [1:0] a_was;
    logic       cs_was, we_was, re_was, wrote, read_end;
    assign d_in     = {p14_in, p13_in, p12_in, p11_in, p10_in, p9_in, p8_in, p7_in};
    assign wrote    = p2 && !we_was && !cs_was;
    assign read_end = p4 && !re_was && !cs_was;

    // ── time and the drive ───────────────────────────────────────────────
    logic clk_was, us, ip_was, index, ready_was;
    assign us    = p24 && !clk_was;                 // a CLK period: 1 us at 1 MHz
    assign index = !p35 && ip_was;                  // an index hole begins

    // ── the command ──────────────────────────────────────────────────────
    typedef enum logic [4:0] {
        IDLE, T1_LOOP, T1_DIR, T1_PULSE, T1_RATE, SETTLE, HEAD, VERIFY,
        SEARCH, ID_FIELD, DAM_WAIT, READ_DATA, READ_CRC, W_GAP, W_FIELD,
        READ_ADDR, RT_WAIT, READ_TRACK, WT_WAIT, WT_WRITE, DONE
    } state_t;
    typedef enum logic [2:0] { F_PRE, F_SYNC, F_MARK, F_DATA, F_CRC1, F_CRC2, F_TAIL } field_t;
    state_t      state, searching;               // ID_FIELD returns to `searching`
    field_t      field;
    logic        busy, intrq, drq, hld, dirc, step, wg, type1, tg43, restore_pending, stepped;
    logic        s_crc, s_rnf, s_lost, s_wp, s_rt, s_seek;
    logic [19:0] timer;
    logic [2:0]  indexes;
    logic [3:0]  idle_indexes, fi_flags;
    logic [1:0]  syncs, crc_left;
    logic [2:0]  id_pos;
    logic [7:0]  id [4];
    logic [15:0] crc, id_crc;
    logic [10:0] count, length;

    logic       rb_valid, rb_sync, took;
    logic [7:0] rb, wbyte;
    logic [1:0] wkind;

    kr1818vg93_reader reader (
        .clk, .rclk(p26), .raw_n(p27 || p37), .byte_valid(rb_valid), .byte_value(rb), .sync(rb_sync)
    );
    kr1818vg93_writer writer (
        .clk, .clk_pin(p24), .run(wg), .byte_in(wbyte), .kind(wkind), .took, .wd(p31)
    );

    // ── the status and the pins (docs/vg93.md §2, §3.1) ─────────────────
    always_comb begin
        if (type1)
            status = {!p32 || !p19, !p36, hld && p23, s_seek, s_crc, !p34, !p35, busy};
        else
            status = {!p32 || !p19, s_wp, s_rt, s_rnf, s_crc, s_lost, drq, busy};
        case ({p6, p5})
            2'd0:    bus = status;
            2'd1:    bus = tr;
            2'd2:    bus = sr;
            default: bus = dr;
        endcase
    end
    // DAL drives a master tick after -RE and -CS fall and floats a tick after
    // they rise: inside tDACC and the float time (docs/vg93.md), and a
    // flip-flop away from the bus for the FPGA
    logic       driving;
    logic [7:0] out_byte;
    always_ff @(posedge clk) begin
        driving  <= !p3 && !p4;
        out_byte <= !p3 && !p4 ? bus : 8'h00;
    end
    assign {p14, p13, p12, p11, p10, p9, p8, p7} = out_byte;
    assign {p14_oe, p13_oe, p12_oe, p11_oe, p10_oe, p9_oe, p8_oe, p7_oe} = {8{driving}};
    assign {p15, p16, p17, p18} = {step, dirc, 1'b0, 1'b0};    // no precompensation asked
    assign {p25, p28, p29, p30} = {1'b0, hld, tg43, wg};       // RG not modelled
    assign {p38, p39} = {drq, intrq};
    assign p33    = 1'b1;                                       // VFOE not modelled
    assign p33_oe = !wg;

    // the step rate in us at 1 MHz, -TEST high or low (docs/vg93.md §4.1)
    function automatic logic [19:0] rate(input logic [1:0] r, input logic fast);
        case (r)
            2'b00:   return fast ? 20'd368 : 20'd6000;
            2'b01:   return fast ? 20'd380 : 20'd12000;
            2'b10:   return fast ? 20'd396 : 20'd20000;
            default: return fast ? 20'd416 : 20'd30000;
        endcase
    endfunction

    always_ff @(posedge clk) begin
        d_was <= d_in; a_was <= {p6, p5}; cs_was <= p3; we_was <= p2; re_was <= p4;
        clk_was <= p24; ip_was <= p35; ready_was <= p32;
        if (!p19)
            master_reset();
        else begin
            if (read_end) bus_read();
            if (wrote) bus_write();
            interrupts();
            if (restore_pending && !wrote) begin
                restore_pending <= 1'b0;
                start(8'h03);                   // -MR rose: Restore, ready or not
            end else if (state != IDLE)
                run_command();
            else if (index && hld) begin
                idle_indexes <= idle_indexes + 4'd1;
                if (idle_indexes == 4'd14) hld <= 1'b0;     // unloaded after 15
            end
        end
    end

    task automatic master_reset();
        cr <= 8'h03; sr <= 8'h01; tr <= 8'h00;
        {busy, intrq, drq, hld, step, wg, tg43} <= '0;
        {s_crc, s_rnf, s_lost, s_wp, s_rt, s_seek} <= '0;
        fi_flags <= '0; type1 <= 1'b1; state <= IDLE; restore_pending <= 1'b1;
    endtask

    // a register read ends: a status read resets INTRQ, a data read DRQ
    task automatic bus_read();
        if (a_was == 2'd0 && !fi_flags[3]) intrq <= 1'b0;
        if (a_was == 2'd3) drq <= 1'b0;
    endtask

    task automatic bus_write();
        case (a_was)
            2'd0: command(d_was);
            2'd1: tr <= d_was;
            2'd2: sr <= d_was;
            default: begin dr <= d_was; drq <= 1'b0; end
        endcase
    endtask

    // Force Interrupt's conditions: a ready change, an index pulse
    task automatic interrupts();
        if ((fi_flags[0] && p32 && !ready_was) || (fi_flags[1] && !p32 && ready_was) ||
            (fi_flags[2] && index))
            intrq <= 1'b1;
    endtask

    task automatic command(input logic [7:0] c);
        if (c[7:4] == 4'hD) begin                       // Force Interrupt
            fi_flags <= c[3:0];
            if (state == IDLE) type1 <= 1'b1;
            state <= IDLE; busy <= 1'b0; wg <= 1'b0; drq <= 1'b0; step <= 1'b0;
            intrq <= c[3];
            return;
        end
        if (busy) return;                               // not taken while busy
        intrq <= 1'b0;
        fi_flags <= '0;
        start(c);
    endtask

    task automatic start(input logic [7:0] c);
        cr <= c; busy <= 1'b1; drq <= 1'b0; idle_indexes <= '0;
        if (!c[7]) begin                                // Type I
            type1 <= 1'b1; s_crc <= 1'b0; s_seek <= 1'b0; hld <= c[3];
            stepped <= 1'b0;
            if (c[7:4] == 4'h0) begin tr <= 8'hFF; dr <= 8'h00; end
            state <= T1_LOOP;
            return;
        end
        type1 <= 1'b0;
        {s_lost, s_rnf, s_rt, s_wp} <= '0;
        if (c[7:4] != 4'hD && !p32) begin state <= DONE; return; end   // not ready
        hld <= 1'b1;
        tg43 <= tr > 8'd43;
        timer <= p22 && c[2] ? 20'd30000 : 20'd0;       // E: 30 ms at 1 MHz
        state <= SETTLE;
    endtask

    // the next state once the head is settled and loaded
    task automatic head_loaded();
        indexes <= '0; syncs <= '0;
        case (cr[7:4])
            4'h0, 4'h1, 4'h2, 4'h3, 4'h4, 4'h5, 4'h6, 4'h7: begin searching <= VERIFY; state <= VERIFY; end
            4'hA, 4'hB, 4'hF:
                if (!p36) begin s_wp <= 1'b1; state <= DONE; end   // write protected
                else if (cr[7:4] == 4'hF) begin drq <= 1'b1; timer <= 20'd96; state <= WT_WAIT; end
                else begin searching <= SEARCH; state <= SEARCH; end
            4'hC: begin searching <= READ_ADDR; state <= READ_ADDR; end
            4'hE: state <= RT_WAIT;                     // Read Track
            default: begin searching <= SEARCH; state <= SEARCH; end    // Read Sector
        endcase
    endtask

    task automatic run_command();
        case (state)
            T1_LOOP:  type1_loop();
            T1_DIR:   if (us) begin
                          if (timer != 0) timer <= timer - 20'd1;
                          else begin step <= 1'b1; timer <= 20'd4; state <= T1_PULSE; end
                      end
            T1_PULSE: if (us) begin
                          if (timer != 0) timer <= timer - 20'd1;
                          else begin step <= 1'b0; timer <= rate(cr[1:0], !p22) - 20'd28; state <= T1_RATE; end
                      end
            T1_RATE:  if (us) begin
                          if (timer != 0) timer <= timer - 20'd1;
                          else state <= T1_LOOP;
                      end
            SETTLE:   if (us) begin
                          if (timer != 0) timer <= timer - 20'd1;
                          else state <= HEAD;
                      end
            HEAD:     if (p23) head_loaded();           // wait for HLT
            VERIFY, SEARCH, READ_ADDR: search_id();
            ID_FIELD:  id_field();
            DAM_WAIT, READ_DATA, READ_CRC: read_field();
            W_GAP, W_FIELD: write_field();
            RT_WAIT, READ_TRACK: read_track();
            WT_WAIT, WT_WRITE: write_track();
            DONE: begin
                busy <= 1'b0; intrq <= 1'b1; wg <= 1'b0; state <= IDLE;
            end
            default: state <= IDLE;
        endcase
    endtask

    // Type I: decide the next step, or go on to verify (docs/vg93.md §4.3)
    task automatic type1_loop();
        logic seek, in;
        seek = cr[7:5] == 3'b000;                       // Restore, Seek
        in = seek ? dr > tr : (cr[6] ? !cr[5] : dirc);
        if (seek && tr == dr) begin
            if (cr[7:4] == 4'h0 && p34) begin s_seek <= 1'b1; state <= DONE; end  // no track 0
            else type1_end();
            return;
        end
        if (!seek && stepped) begin type1_end(); return; end
        dirc <= in;
        stepped <= 1'b1;
        if (seek || cr[4]) tr <= in ? tr + 8'd1 : tr - 8'd1;
        if (!p34 && !in) begin                          // at track 0, stepping out
            tr <= 8'd0;
            type1_end();
            return;
        end
        timer <= 20'd24;                                // DIRC before STEP
        state <= T1_DIR;
    endtask

    task automatic type1_end();
        if (cr[2]) begin                                // verify
            hld <= 1'b1;
            timer <= p22 ? 20'd30000 : 20'd0;
            state <= SETTLE;
        end else
            state <= DONE;
    endtask

    // Looking for an ID field: three A1s and FE; a timeout of 5 index
    // pulses (6 for Read Address)
    task automatic search_id();
        if (index) begin
            indexes <= indexes + 3'd1;
            if (indexes == (state == READ_ADDR ? 3'd5 : 3'd4)) begin
                if (state == VERIFY) s_seek <= 1'b1; else s_rnf <= 1'b1;
                state <= DONE;
                return;
            end
        end
        if (!rb_valid) return;
        if (rb_sync) syncs <= syncs == 2'd3 ? 2'd3 : syncs + 2'd1;
        else if (syncs == 2'd3 && rb == 8'hFE) begin
            crc <= crc_byte(crc_byte(crc_byte(crc_byte(16'hFFFF, 8'hA1), 8'hA1), 8'hA1), 8'hFE);
            id_pos <= '0;
            state <= ID_FIELD;
        end else
            syncs <= '0;
    endtask

    task automatic id_field();
        if (!rb_valid) return;
        if (id_pos < 3'd4) id[id_pos[1:0]] <= rb;
        if (id_pos < 3'd4) crc <= crc_byte(crc, rb);
        if (searching == READ_ADDR) deliver(rb);
        if (id_pos == 3'd4) id_crc[15:8] <= rb;
        if (id_pos != 3'd5) begin id_pos <= id_pos + 3'd1; return; end
        id_crc[7:0] <= rb;
        id_checked({id_crc[15:8], rb});
    endtask

    // an ID field complete: its CRC and its numbers against the registers
    task automatic id_checked(input logic [15:0] got);
        logic ok;
        ok = got == crc;
        syncs <= '0;
        state <= searching;
        if (!ok) begin s_crc <= 1'b1; return; end
        case (searching)
            VERIFY: begin
                s_crc <= 1'b0;
                if (id[0] != tr) s_seek <= 1'b1;
                state <= DONE;
            end
            READ_ADDR: begin sr <= id[0]; s_crc <= 1'b0; state <= DONE; end
            default:
                if (id[0] == tr && id[2] == sr && (!cr[1] || id[1][0] == cr[3])) begin
                    s_crc  <= 1'b0;
                    length <= 11'd128 << id[3][1:0];
                    count  <= '0;
                    if (cr[5]) begin timer <= 20'd0; state <= W_GAP; end
                    else state <= DAM_WAIT;
                end
        endcase
    endtask

    // a byte to the CPU: DR and DRQ, LOST DATA if the last was not taken
    task automatic deliver(input logic [7:0] b);
        if (drq) s_lost <= 1'b1;
        dr  <= b;
        drq <= 1'b1;
    endtask

    // Read Sector: the data mark within 43 bytes, the data, its CRC
    task automatic read_field();
        if (!rb_valid) return;
        case (state)
            DAM_WAIT: begin
                count <= count + 11'd1;
                if (rb_sync) syncs <= syncs == 2'd3 ? 2'd3 : syncs + 2'd1;
                else if (syncs == 2'd3 && (rb == 8'hFB || rb == 8'hF8)) begin
                    s_rt  <= rb == 8'hF8;
                    crc   <= crc_byte(crc_byte(crc_byte(crc_byte(16'hFFFF, 8'hA1), 8'hA1), 8'hA1), rb);
                    count <= '0;
                    state <= READ_DATA;
                end else begin
                    syncs <= '0;
                    if (count == 11'd43) begin s_rnf <= 1'b1; state <= DONE; end
                end
            end
            READ_DATA: begin
                deliver(rb);
                crc   <= crc_byte(crc, rb);
                count <= count + 11'd1;
                if (count + 11'd1 == length) begin count <= '0; state <= READ_CRC; end
            end
            default: begin                              // READ_CRC
                id_crc <= {id_crc[7:0], rb};
                count  <= count + 11'd1;
                if (count == 11'd1) begin
                    if ({id_crc[7:0], rb} != crc) begin s_crc <= 1'b1; state <= DONE; end
                    else if (cr[4]) begin sr <= sr + 8'd1; indexes <= '0; syncs <= '0; state <= SEARCH; end
                    else state <= DONE;
                end
            end
        endcase
    endtask

    // the next byte to write: kind, value, into the CRC
    task automatic schedule(input logic [1:0] k, input logic [7:0] b, input logic [15:0] c);
        wkind <= k;
        wbyte <= b;
        crc   <= c;
    endtask

    // the DR's byte for the writer: 00 and LOST DATA if the CPU did not load it
    function automatic logic [7:0] dr_or_zero();
        return drq ? 8'h00 : dr;
    endfunction

    // Write Sector: 22 bytes after the ID, DRQ at 2 and checked at 10, then
    // 12 00s, three A1s, the mark, the data, the CRC and an FF (docs §4.4)
    task automatic write_field();
        if (wg && !p33_in) begin s_rt <= 1'b1; state <= DONE; return; end      // write fault
        if (state == W_GAP) begin
            if (!us) return;
            timer <= timer + 20'd1;
            if (timer == 20'd64) drq <= 1'b1;
            if (timer == 20'd320 && drq) begin s_lost <= 1'b1; state <= DONE; end
            if (timer == 20'd704) begin
                wg <= 1'b1; field <= F_PRE; count <= '0;
                schedule(2'd0, 8'h00, crc);
                state <= W_FIELD;
            end
            return;
        end
        if (!took) return;
        count <= count + 11'd1;
        case (field)
            F_PRE:
                if (count == 11'd11) begin
                    field <= F_SYNC; count <= '0;
                    schedule(2'd1, 8'hA1, crc_byte(16'hFFFF, 8'hA1));
                end else schedule(2'd0, 8'h00, crc);
            F_SYNC:
                if (count == 11'd2) begin
                    field <= F_MARK;
                    schedule(2'd0, cr[0] ? 8'hF8 : 8'hFB, crc_byte(crc, cr[0] ? 8'hF8 : 8'hFB));
                end else schedule(2'd1, 8'hA1, crc_byte(crc, 8'hA1));
            F_MARK: begin
                field <= F_DATA; count <= '0;
                if (drq) s_lost <= 1'b1;
                schedule(2'd0, dr_or_zero(), crc_byte(crc, dr_or_zero()));
                drq <= length != 11'd1;
            end
            F_DATA:
                if (count + 11'd1 == length) begin
                    field <= F_CRC1;
                    schedule(2'd0, crc[15:8], crc);
                end else begin
                    if (drq) s_lost <= 1'b1;
                    schedule(2'd0, dr_or_zero(), crc_byte(crc, dr_or_zero()));
                    drq <= count + 11'd2 != length;
                end
            F_CRC1: begin field <= F_CRC2; schedule(2'd0, crc[7:0], crc); end
            F_CRC2: begin field <= F_TAIL; schedule(2'd0, 8'hFF, crc); end
            default: begin                              // the FF is out: WG falls
                wg <= 1'b0;
                if (cr[4]) begin sr <= sr + 8'd1; indexes <= '0; syncs <= '0; state <= SEARCH; end
                else state <= DONE;
            end
        endcase
    endtask

    // Read Track: every byte from one index pulse to the next
    task automatic read_track();
        if (state == RT_WAIT) begin
            if (index) state <= READ_TRACK;
        end else if (index)
            state <= DONE;
        else if (rb_valid)
            deliver(rb);
    endtask

    // Write Track: DRQ at once, the first byte within 3 byte times, then
    // index to index; F5 is A1 (the CRC preset at the first), F6 C2, F7
    // the two CRC bytes (docs §4.5)
    task automatic write_track();
        if (state == WT_WAIT) begin
            if (us && timer != 0) begin
                timer <= timer - 20'd1;
                if (timer == 20'd1 && drq) begin s_lost <= 1'b1; state <= DONE; end
            end
            if (index && !drq) begin
                wg <= 1'b1; crc_left <= '0; state <= WT_WRITE;
                track_byte(dr, 1'b0);
            end
            return;
        end
        if (wg && !p33_in) begin s_rt <= 1'b1; state <= DONE; return; end
        if (index) begin wg <= 1'b0; state <= DONE; return; end
        if (!took) return;
        if (crc_left == 2'd1) begin
            crc_left <= '0;
            schedule(2'd0, crc[7:0], crc);
        end else
            track_byte(dr_or_zero(), drq);
    endtask

    // one DR byte of Write Track to the writer
    task automatic track_byte(input logic [7:0] b, input logic lost);
        if (lost) s_lost <= 1'b1;
        drq <= 1'b1;
        case (b)
            8'hF5:   schedule(2'd1, 8'hA1, crc_byte(wkind == 2'd1 ? crc : 16'hFFFF, 8'hA1));
            8'hF6:   schedule(2'd2, 8'hC2, crc);
            8'hF7:   begin crc_left <= 2'd1; schedule(2'd0, crc[15:8], crc); end
            default: schedule(2'd0, b, crc_byte(crc, b));
        endcase
    endtask
endmodule
