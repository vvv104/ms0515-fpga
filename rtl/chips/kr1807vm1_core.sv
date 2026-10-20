// The execution side of КР1807ВМ1 (see kr1807vm1.sv): the registers, the
// PS, and each instruction as the steps it takes on the bus, one
// transaction a step (kr1807vm1_steps_pkg).  A step's request is on the
// bus's inputs while it is next; the step acts when the bus has taken it -
// a read when its data arrive, an IACK when its vector does.  Between
// instructions the requests read in the reads and ASPIs decide what comes
// next: HALT, power fail, the trace trap, an interrupt, or the next fetch.
module kr1807vm1_core (
    input  logic        clk,
    input  logic        run,            // PUP low
    // the bus
    input  logic        phase,
    input  logic        take,
    input  logic        data_valid,
    input  logic [15:0] data,
    input  logic        ai_valid,
    input  logic        ai_requests,
    input  logic [7:0]  ai,
    input  logic        vector_valid,
    input  logic [4:0]  mode_in,        // DAL 15:13, 8, 0: the mode while -BCLR is low
    output logic [2:0]  req_kind,
    output logic        req_fetch,
    output logic [15:0] req_addr,
    output logic [15:0] req_data,
    output logic [1:0]  req_bytes,
    output logic [4:0]  req_ack,
    output logic        bclr,
    output logic        delayed,        // MR8
    output logic        cout_pulse      // MR0
);
    import kr1807vm1_pkg::*;
    import kr1807vm1_steps_pkg::*;

    // ── state ───────────────────────────────────────────────────────────
    logic [15:0] r [8];
    logic [7:0]  ps;
    logic [15:0] ir, x, ea_s, ea_d, src, dst, res;
    logic [4:0]  mr;                    // MR15:13, MR8, MR0

    // the instruction (or service) being executed
    kind_t       kind;
    alu_op_t     aop;
    logic        bw, writes;
    logic [2:0]  smode, sreg, dmode, dreg;
    logic [7:0]  vec;
    step_t       prog [16];
    logic [4:0]  len, upc;              // the steps, and the next one's index
    logic [7:0]  rep;
    step_t       cur;                   // the step on the bus

    // requests and events between instructions
    logic [5:1]  seen;                  // -VEC, -CP0..-CP3 as last read in a read or ASPI
    logic        hlt_armed, pf_armed, halt_req, pf_req, trace, dma;
    logic        bclr_pup, bclr_reset;
    logic [6:0]  t_count;               // T since PUP fell, up to 100

    assign bclr       = bclr_pup || bclr_reset;
    assign delayed    = mr[1];
    assign cout_pulse = mr[0];

    // ── requests (AI: -HLT -PF -VEC -CP0 -CP1 -CP2 -CP3 -DMR) ───────────
    function automatic logic [2:0] level_of(input logic [4:1] a);    // -CP0..-CP3
        if (!a[1]) return a[2] ? 3'd6 : 3'd7;
        if (!a[2]) return 3'd5;
        return a[4:3] == 2'b11 ? 3'd0 : 3'd4;
    endfunction

    function automatic logic [7:0] vector_of(input logic [4:1] a);
        logic [7:0] base;
        case (level_of(a))
            3'd7:    base = 8'o140;
            3'd6:    base = 8'o100;
            3'd5:    base = 8'o120;
            default: base = 8'o060;
        endcase
        return base + {4'd0, a[3], a[4], 2'b00};    // CP1, CP0
    endfunction

    serve_t serve;
    always_comb begin
        if (halt_req)                                       serve = S_HALT;
        else if (pf_req)                                    serve = S_PF;
        else if (trace)                                     serve = S_TRACE;
        else if (level_of(seen[4:1]) != 0 && level_of(seen[4:1]) > ps[7:5]) serve = S_INT;
        else                                                serve = S_FETCH;
    end

    // ── the next step and its request ───────────────────────────────────
    prog_t next_service;
    step_t nxt;
    logic  boundary, looping, dmg;
    assign boundary     = upc == len;
    // WAIT goes round its loop while nothing is to be served
    assign looping      = boundary && cur == U_WAIT && serve == S_FETCH;
    assign next_service = service(serve, serve == S_INT && !seen[5]);
    assign nxt          = looping ? prog[1] : boundary ? next_service.ops[0] : prog[upc[3:0]];
    // a DMA grant comes between transactions, never inside a read-modify-write
    assign dmg          = dma && !(cur == U_RD_D && nxt == U_WR_D);

    logic [15:0] inc_s, inc_d;          // autoincrement and -decrement steps
    logic [15:0] at_s, at_d;            // the operand addresses read at
    assign inc_s = (bw && sreg < 3'd6) ? 16'd1 : 16'd2;
    assign inc_d = (bw && dreg < 3'd6) ? 16'd1 : 16'd2;
    assign at_s  = (smode == 3'd1 || smode == 3'd2) ? r[sreg] : ea_s;
    assign at_d  = (dmode == 3'd1 || dmode == 3'd2) ? r[dreg] : ea_d;

    function automatic logic [15:0] even(input logic [15:0] a);
        return a & 16'hFFFE;
    endfunction

    always_comb begin
        req_kind  = BUS_NOP;
        req_fetch = 1'b0;
        req_addr  = '0;
        req_data  = '0;
        req_bytes = 2'b11;
        req_ack   = {seen[5], seen[4], seen[3], seen[2], seen[1]};
        if (dmg)
            req_kind = BUS_DMG;
        else case (nxt)
            U_FETCH:          begin req_kind = BUS_READ; req_fetch = 1'b1; req_addr = even(r[7]); end
            U_IDX_S, U_IDX_D: begin req_kind = BUS_READ; req_addr = even(r[7]); end
            U_PTR_S: begin req_kind = BUS_READ; req_addr = even(smode == 3'd3 ? r[sreg] : ea_s); end
            U_PTR_D: begin req_kind = BUS_READ; req_addr = even(dmode == 3'd3 ? r[dreg] : ea_d); end
            U_RD_S:  begin req_kind = BUS_READ; req_addr = bw ? at_s : even(at_s); end
            U_RD_D:  begin req_kind = BUS_READ; req_addr = bw ? at_d : even(at_d); end
            U_WR_D: begin
                req_kind  = BUS_WRITE;
                req_addr  = bw ? ea_d : even(ea_d);
                req_data  = bw ? {res[7:0], res[7:0]} : res;
                req_bytes = bw ? (ea_d[0] ? 2'b10 : 2'b01) : 2'b11;
            end
            U_PUSH_PS: begin req_kind = BUS_WRITE; req_addr = even(r[6] - 16'd2); req_data = {8'd0, ps}; end
            U_PUSH_PC: begin req_kind = BUS_WRITE; req_addr = even(r[6] - 16'd2); req_data = r[7]; end
            U_PUSH_T:  begin req_kind = BUS_WRITE; req_addr = even(r[6] - 16'd2); req_data = r[sreg]; end
            U_VEC_PC:  begin req_kind = BUS_READ; req_addr = {8'd0, vec}; end
            U_VEC_PS:  begin req_kind = BUS_READ; req_addr = {8'd0, vec} + 16'd2; end
            U_POP_PC, U_POP_PS, U_POP_R: begin req_kind = BUS_READ; req_addr = even(r[6]); end
            U_ASPI, U_WAIT, U_START: req_kind = BUS_ASPI;
            U_IACK:    req_kind = BUS_IACK;
            default: ;
        endcase
    end

    // ── the steps' effects ──────────────────────────────────────────────
    function automatic logic [15:0] start_address(input logic [2:0] m);
        case (m)
            3'b111:  return 16'o172000;
            3'b110:  return 16'o173000;
            3'b101:  return 16'o000000;
            3'b100:  return 16'o010000;
            3'b011:  return 16'o020000;
            3'b010:  return 16'o040000;
            3'b001:  return 16'o100000;
            default: return 16'o140000;
        endcase
    endfunction

    // A register source is read when the operation uses it, after the
    // destination's address steps: so `MOV R0,(R0)+` stores R0 + 2 and
    // `MOV PC,X(R)` the opcode's address + 4, as the T-11 does.
    logic [15:0] src_val;
    assign src_val = smode != 0 ? src : (aop == A_MFPS ? {8'd0, ps} : r[sreg]);

    // a word read, or the byte the address's low bit picks
    function automatic logic [15:0] pick(input logic [15:0] w, input logic odd, input logic b);
        return b ? {8'd0, odd ? w[15:8] : w[7:0]} : w;
    endfunction

    alu_t operated;                     // the operation on a register or the value read
    assign operated = alu(aop, bw, src_val, dmode == 0 ? r[dreg] : dst, ps[3], ps[0]);

    // an ALU result into a register: a byte into the low half, MOVB and
    // MFPS sign-extending
    task automatic to_register(input logic [2:0] n, input logic [15:0] v);
        if (!bw)
            r[n] <= v;
        else if (aop == A_MOV || aop == A_MFPS)
            r[n] <= {{8{v[7]}}, v[7:0]};
        else
            r[n] <= {r[n][15:8], v[7:0]};
    endtask

    function automatic logic branch_taken(input logic [3:0] c, input logic [3:0] cc);
        logic n, z, v, cy;
        {n, z, v, cy} = cc;
        case (c)
            4'b0001: return 1'b1;               // BR
            4'b0010: return !z;                 // BNE
            4'b0011: return z;                  // BEQ
            4'b0100: return !(n ^ v);           // BGE
            4'b0101: return n ^ v;              // BLT
            4'b0110: return !(z || (n ^ v));    // BGT
            4'b0111: return z || (n ^ v);       // BLE
            4'b1000: return !n;                 // BPL
            4'b1001: return n;                  // BMI
            4'b1010: return !(cy || z);         // BHI
            4'b1011: return cy || z;            // BLOS
            4'b1100: return !v;                 // BVC
            4'b1101: return v;                  // BVS
            4'b1110: return !cy;                // BCC
            4'b1111: return cy;                 // BCS
            default: return 1'b0;
        endcase
    endfunction

    task automatic execute();
        case (kind)
            K_ALU:
                if (dmode == 0) begin
                    ps[3:0] <= operated.nzvc;
                    if (writes) to_register(dreg, operated.res);
                end else
                    ps[3:0] <= operated.nzvc;
            K_BR:   if (branch_taken({ir[15], ir[10:8]}, ps[3:0]))
                        r[7] <= r[7] + {{7{ir[7]}}, ir[7:0], 1'b0};
            K_SOB: begin
                r[sreg] <= r[sreg] - 16'd1;
                if (r[sreg] != 16'd1)
                    r[7] <= r[7] - {9'd0, ir[5:0], 1'b0};
            end
            K_JMP:  r[7] <= ea_d;
            K_JSR:  begin r[sreg] <= r[7]; r[7] <= ea_d; end
            K_RTS:  r[7] <= r[dreg];
            K_CCC:  ps[3:0] <= ir[4] ? ps[3:0] | ir[3:0] : ps[3:0] & ~ir[3:0];
            K_MFPT: r[0] <= 16'd4;
            K_MTPS: ps <= {src_val[7:5], ps[4], src_val[3:0]};     // T is kept
            K_HALT: begin r[7] <= start_address(mr[4:2]) + 16'd4; ps <= 8'o340; end
            K_RTI:  trace <= ps[4];             // T set by RTI traps at once
            default: ;
        endcase
    endtask

    // a step that acts when the bus takes it
    task automatic at_take(input step_t s);
        case (s)
            U_DECODE: load(decode(ir));
            U_SUM_S:
                if (smode == 3'd4 || smode == 3'd5) begin
                    r[sreg] <= r[sreg] - (smode == 3'd5 ? 16'd2 : inc_s);
                    ea_s    <= r[sreg] - (smode == 3'd5 ? 16'd2 : inc_s);
                end else
                    ea_s <= r[sreg] + x;
            U_SUM_D:
                if (dmode == 3'd4 || dmode == 3'd5) begin
                    r[dreg] <= r[dreg] - (dmode == 3'd5 ? 16'd2 : inc_d);
                    ea_d    <= r[dreg] - (dmode == 3'd5 ? 16'd2 : inc_d);
                end else
                    ea_d <= r[dreg] + x;
            U_EA_D: begin
                ea_d <= r[dreg];
                if (dmode == 3'd2) r[dreg] <= r[dreg] + 16'd2;
            end
            U_EXEC:     execute();
            U_PUSH_PS, U_PUSH_PC, U_PUSH_T: r[6] <= r[6] - 16'd2;
            U_BCLR_ON:  bclr_reset <= 1'b1;
            U_BCLR_OFF: bclr_reset <= 1'b0;
            U_START: begin
                r[6] <= 16'o376;
                r[7] <= start_address(mr[4:2]);
                ps   <= 8'o340;
            end
            default: ;
        endcase
    endtask

    // a read step, when its data arrive
    task automatic at_data(input step_t s, input logic [15:0] d);
        alu_t a;
        case (s)
            U_FETCH: begin
                ir    <= d;
                r[7]  <= r[7] + 16'd2;
                trace <= ps[4];                 // T at the start: trace after it
            end
            U_IDX_S, U_IDX_D: begin x <= d; r[7] <= r[7] + 16'd2; end
            U_PTR_S: begin
                ea_s <= d;
                if (smode == 3'd3) r[sreg] <= r[sreg] + 16'd2;
            end
            U_PTR_D: begin
                ea_d <= d;
                if (dmode == 3'd3) r[dreg] <= r[dreg] + 16'd2;
            end
            U_RD_S: begin
                src <= pick(d, at_s[0], bw);
                if (smode == 3'd2) r[sreg] <= r[sreg] + inc_s;
            end
            U_RD_D: begin
                dst  <= pick(d, at_d[0], bw);
                ea_d <= at_d;
                if (dmode == 3'd2) r[dreg] <= r[dreg] + inc_d;
                if (writes) begin
                    a = alu(aop, bw, src_val, pick(d, at_d[0], bw), ps[3], ps[0]);
                    res     <= a.res;
                    ps[3:0] <= a.nzvc;
                end
            end
            U_VEC_PC: r[7] <= d;
            U_VEC_PS: ps <= d[7:0];
            U_POP_PC: begin r[7] <= d; r[6] <= r[6] + 16'd2; end
            U_POP_PS: begin ps <= d[7:0]; r[6] <= r[6] + 16'd2; end
            U_POP_R:  begin r[dreg] <= d; r[6] <= r[6] + 16'd2; end
            default: ;
        endcase
    endtask

    // an instruction's steps, from the first
    task automatic load(input decoded_t d);
        kind   <= d.kind;
        aop    <= d.aop;
        bw     <= d.bw;
        writes <= d.writes;
        smode  <= d.smode;
        sreg   <= d.sreg;
        dmode  <= d.dmode;
        dreg   <= d.dreg;
        vec    <= d.vec;
        rep    <= d.rep;
        for (int i = 0; i < 16; i++)
            prog[i] <= d.prog.ops[i];
        len <= d.prog.len;
        upc <= 5'd0;
    endtask

    // a service begins: its steps from the second on, its vector
    task automatic begin_service(input prog_t p);
        for (int i = 0; i < 16; i++)
            prog[i] <= p.ops[i];
        len <= p.len;
        upc <= 5'd1;
        case (serve)
            S_PF:    begin vec <= 8'o24; pf_req <= 1'b0; end
            S_TRACE: begin vec <= 8'o14; trace <= 1'b0; end
            S_INT:   vec <= vector_of(seen[4:1]);
            S_HALT:  begin halt_req <= 1'b0; kind <= K_HALT; end
            default: ;
        endcase
    endtask

    // the bus has taken the next step
    task automatic took();
        if (dmg)
            cur <= U_N;
        else if (looping) begin
            cur <= nxt;
            upc <= 5'd2;
        end else if (boundary) begin
            begin_service(next_service);
            cur <= nxt;
            at_take(nxt);
        end else begin
            cur <= nxt;
            if (nxt == U_REP && rep != 0)
                rep <= rep - 8'd1;
            else
                upc <= upc + 5'd1;
            at_take(nxt);
        end
    endtask

    // AI at a PI trailing edge: -DMR always, the requests in reads and ASPIs
    task automatic sampled();
        dma <= !ai[0];
        if (ai_requests) begin
            seen <= ai[5:1];
            if (ai[7]) hlt_armed <= 1'b1;
            else if (hlt_armed) begin halt_req <= 1'b1; hlt_armed <= 1'b0; end
            if (ai[6]) pf_armed <= 1'b1;
            else if (pf_armed) begin pf_req <= 1'b1; pf_armed <= 1'b0; end
        end
    endtask

    always_ff @(posedge clk) begin
        if (!run) begin
            // power-up: 100 bus NOPs while -BCLR goes up at 100T, then the
            // ASPI that loads SP, PC and PS; the first fetch at 304T
            prog[0] <= U_REP;
            prog[1] <= U_START;
            len <= 5'd2; upc <= 5'd0; rep <= 8'd99;
            cur <= U_N; kind <= K_NONE;
            bclr_pup <= 1'b1; bclr_reset <= 1'b0; t_count <= '0;
            mr <= 5'b11111;
            seen <= 5'h1F; hlt_armed <= 1'b0; pf_armed <= 1'b0;
            halt_req <= 1'b0; pf_req <= 1'b0; trace <= 1'b0; dma <= 1'b0;
        end else begin
            if (phase && bclr_pup) begin
                t_count <= t_count + 7'd1;
                if (t_count == 7'd99)
                    bclr_pup <= 1'b0;
            end
            if (bclr)
                mr <= mode_in;
            if (take)
                took();
            if (data_valid)
                at_data(cur, data);
            if (vector_valid && !seen[5])
                vec <= {data[7:2], 2'b00};
            if (ai_valid)
                sampled();
        end
    end
endmodule
