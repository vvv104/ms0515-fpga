// КМ1816ВЕ48 (8048/8035): the MCS-48 single-chip microcomputer at its pins,
// as the МС 7004 uses it - every fetch from outside (EA high on the board),
// its program through BUS, ALE and PSEN, the 8243 on P20..P23 and PROG.
// Built from the MCS-48 Family User's Manual (1980) and its 8048H data
// sheet: docs/km1816ve48.md says where each edge comes from.
//
// The crystal on XTAL1 (pin 2), seen on the master tick, drives everything:
// a machine cycle is its 30 half-periods, h 0..29 - S1 h 0..5, S2 6..11, S3
// 12..17, S4 18..23, S5 24..29.  Each cycle has a slot - the address on BUS
// in S4 with ALE, a strobe low across S5 and S1 - that brings the byte the
// next cycle takes as the strobe rises (h 9): with PSEN the next opcode or
// a 2-cycle instruction's second byte; a cycle with nothing to fetch lends
// its slot to PROG (MOVD, ANLD, ORLD) or to RD and WR (MOVX, INS).  What an
// instruction does at the pins it does in the state the manual's
// "Instruction timing diagram" (p. 2-11) gives; its registers change in S3
// of its last cycle, out of sight.
//
// Ports 1 and 2 are quasi-bidirectional on the chip; here they drive both
// levels and read back what their pins carry - on the МС 7004 nothing else
// drives them but the 8243 during a read, when P20..P23 let go.  T0 is an
// input only (ENT0 CLK's clock output is not modelled: the МС 7004 leaves
// T0 unconnected); EA and SS are on +5 V there, not ports.
module km1816ve48
    import km1816ve48_pkg::*;
(
    input  logic clk,
    input  logic p2,                    // XTAL1
    output logic p3,                    // XTAL2: the oscillator's output
    input  logic p4,                    // RESET (low)
    input  logic p6,                    // INT (low)
    input  logic p1,  input  logic p39, // T0 T1
    output logic p8,  output logic p9,  output logic p10, output logic p11,  // RD PSEN WR ALE
    output logic p25,                   // PROG
    input  logic p12_in, input  logic p13_in, input  logic p14_in, input  logic p15_in,  // BUS
    input  logic p16_in, input  logic p17_in, input  logic p18_in, input  logic p19_in,
    output logic p12, output logic p13, output logic p14, output logic p15,
    output logic p16, output logic p17, output logic p18, output logic p19,
    output logic p12_oe, output logic p13_oe, output logic p14_oe, output logic p15_oe,
    output logic p16_oe, output logic p17_oe, output logic p18_oe, output logic p19_oe,
    input  logic p27_in, input  logic p28_in, input  logic p29_in, input  logic p30_in,  // P10..P17
    input  logic p31_in, input  logic p32_in, input  logic p33_in, input  logic p34_in,
    output logic p27, output logic p28, output logic p29, output logic p30,
    output logic p31, output logic p32, output logic p33, output logic p34,
    output logic p27_oe, output logic p28_oe, output logic p29_oe, output logic p30_oe,
    output logic p31_oe, output logic p32_oe, output logic p33_oe, output logic p34_oe,
    input  logic p21_in, input  logic p22_in, input  logic p23_in, input  logic p24_in,  // P20..P23
    input  logic p35_in, input  logic p36_in, input  logic p37_in, input  logic p38_in,  // P24..P27
    output logic p21, output logic p22, output logic p23, output logic p24,
    output logic p35, output logic p36, output logic p37, output logic p38,
    output logic p21_oe, output logic p22_oe, output logic p23_oe, output logic p24_oe,
    output logic p35_oe, output logic p36_oe, output logic p37_oe, output logic p38_oe
);
    // ---- the crystal: its half-periods h, the cycle ---------------------------
    logic       xtal_was, half;
    logic [4:0] h, h_n;
    assign p3 = !p2;
    assign half = p2 != xtal_was;
    assign h_n = h == 5'd29 ? 5'd0 : h + 5'd1;
    always_ff @(posedge clk) begin
        xtal_was <= p2;
        if (half) h <= h_n;
    end
    // an event as h becomes n
    function automatic logic at(logic [4:0] n);
        return half && h_n == n;
    endfunction

    // ---- the pins as read -------------------------------------------------------
    logic [7:0] bus_pins, p1_pins, p2_pins;
    assign bus_pins = {p19_in, p18_in, p17_in, p16_in, p15_in, p14_in, p13_in, p12_in};
    assign p1_pins = {p34_in, p33_in, p32_in, p31_in, p30_in, p29_in, p28_in, p27_in};
    assign p2_pins = {p38_in, p37_in, p36_in, p35_in, p24_in, p23_in, p22_in, p21_in};

    // ---- the machine --------------------------------------------------------------
    logic [11:0] pc;
    logic [7:0]  a, ir, imm, t, held;   // held: a port read in cycle 1 for cycle 2
    logic        cy, ac, f0, f1, bs, dbf;
    logic [2:0]  sp;
    logic [7:0]  ram [64];
    logic [7:0]  port1, port2, bus_latch;
    logic        bus_latched;
    logic        ie, tie, in_int, int_low, tf, t_req;
    logic [1:0]  tmode;                 // 0 stopped, 1 timer, 2 counter
    logic [4:0]  prescale;
    logic        t1_was;
    // what this cycle is: idle (after reset, before the first opcode), a
    // 1-cycle instruction, the first or second cycle of a 2-cycle one, the
    // two cycles of an interrupt's inserted CALL
    typedef enum logic [2:0] {P_IDLE, P_ONE, P_C1, P_C2, P_V1, P_V2} phase_t;
    phase_t      phase, next_phase;
    logic [11:0] vector;
    logic        cond;                  // a conditional jump's condition, from S3 of cycle 1
    logic [3:0]  imm_page;              // the page the second byte came from

    typedef enum logic [2:0] {S_IDLE, S_OPCODE, S_IMM, S_TABLE, S_PROG, S_READ, S_WRITE} slot_t2;
    slot_t2      slot;                  // from h 19 to h 9 of the next cycle
    logic [11:0] slot_addr;
    logic        slot_bus;              // the slot puts an address on BUS (not INS)
    logic [1:0]  order;                 // the 8243's operation: 0 read, 1 write, 2 OR, 3 AND

    logic [5:0]  r_at, ri_at;           // Rr's location, the location @Rr names
    assign r_at = bs ? 6'd24 + 6'(ir[2:0]) : 6'(ir[2:0]);
    assign ri_at = ram[bs ? 6'd24 + 6'(ir[0]) : 6'(ir[0])][5:0];

    logic [7:0] psw;
    assign psw = {cy, ac, f0, bs, 1'b1, sp};

    // ---- the accumulator's instructions, one cycle (4.1) -------------------
    typedef struct packed {
        logic [7:0] a;
        logic       cy, ac;
    } acc_t;

    function automatic acc_t accumulate(logic [3:0] op, logic [7:0] x);
        acc_t o;
        sum_t s;
        o.a = a; o.cy = cy; o.ac = ac;
        case (op)
            4'h6: begin s = add(a, x, 1'b0); o.a = s.a; o.cy = s.cy; o.ac = s.ac; end      // ADD
            4'h7: begin s = add(a, x, cy); o.a = s.a; o.cy = s.cy; o.ac = s.ac; end         // ADDC
            4'h5: o.a = a & x;                                                              // ANL
            4'h4: o.a = a | x;                                                              // ORL
            4'hD: o.a = a ^ x;                                                              // XRL
            default: ;
        endcase
        return o;
    endfunction

    // the immediate forms: ADD A,# 03, ADDC 13, ORL 43, ANL 53, XRL D3
    function automatic acc_t accumulate_imm(logic [7:0] op, logic [7:0] x);
        case (op)
            8'h03:   return accumulate(4'h6, x);
            8'h13:   return accumulate(4'h7, x);
            8'h43:   return accumulate(4'h4, x);
            8'h53:   return accumulate(4'h5, x);
            default: return accumulate(4'hD, x);
        endcase
    endfunction

    // CLR CPL INC DEC DA SWAP RL RLC RR RRC A, CLR CPL C
    function automatic acc_t unary(logic [7:0] op);
        acc_t o;
        o.a = a; o.cy = cy; o.ac = ac;
        case (op)
            8'h27: o.a = 8'h00;
            8'h37: o.a = ~a;
            8'h17: o.a = a + 8'd1;
            8'h07: o.a = a - 8'd1;
            8'h57: {o.cy, o.a} = decimal(a, cy, ac);
            8'h47: o.a = {a[3:0], a[7:4]};
            8'hE7: o.a = {a[6:0], a[7]};
            8'hF7: begin o.a = {a[6:0], cy}; o.cy = a[7]; end
            8'h77: o.a = {a[0], a[7:1]};
            8'h67: begin o.a = {cy, a[7:1]}; o.cy = a[0]; end
            8'h97: o.cy = 1'b0;
            8'hA7: o.cy = !cy;
            default: ;
        endcase
        return o;
    endfunction

    // a conditional jump's condition (sampled in S3 of its first cycle)
    function automatic logic jump_taken(logic [7:0] op);
        casez (op)
            8'b???1_0010: return a[op[7:5]];                // JBb
            8'hF6: return cy;           8'hE6: return !cy;
            8'hC6: return a == 8'd0;    8'h96: return a != 8'd0;
            8'h36: return p1;           8'h26: return !p1;
            8'h56: return p39;          8'h46: return !p39;
            8'hB6: return f0;           8'h76: return f1;
            8'h16: return tf;           8'h86: return !p6;
            default: return 1'b1;                           // JMP, CALL, DJNZ (decided later)
        endcase
    endfunction

    // the stack (RAM 8..23): PC and the PSW's upper half, pushed and popped
    function automatic logic [5:0] stack_at(logic [2:0] s);
        return 6'd8 + {2'd0, s, 1'b0};
    endfunction

    // the next PC: the low 11 bits count, bit 11 stays (3.1)
    function automatic logic [11:0] step(logic [11:0] p);
        return {p[11], p[10:0] + 11'd1};
    endfunction

    // ---- the machine, edge by edge of the crystal ----------------------------
    always_ff @(posedge clk) begin
        if (!p4) begin                                          // reset (2.1.12)
            pc <= '0;
            sp <= '0;
            bs <= 1'b0;
            dbf <= 1'b0;
            f0 <= 1'b0;
            f1 <= 1'b0;
            port1 <= 8'hFF;
            port2 <= 8'hFF;
            bus_latched <= 1'b0;
            ie <= 1'b0;
            tie <= 1'b0;
            in_int <= 1'b0;
            tmode <= 2'd0;
            tf <= 1'b0;
            t_req <= 1'b0;
            phase <= P_IDLE;                                    // the opcode at 0 fetched next
            next_phase <= P_IDLE;
            if (at(5'd10)) slot <= S_IDLE;
        end else begin
            if (at(5'd9)) byte_in();
            if (at(5'd10) && slot != S_IDLE) slot <= S_IDLE;
            if (at(5'd12)) begin
                if (phase == P_ONE || phase == P_C2) execute(phase == P_C2);
                else if (phase == P_C1) prepare();
                else if (phase == P_V2) take_vector();
            end
            if (at(5'd17)) int_low <= !p6;                      // INT sampled with ALE (2.1)
            if (at(5'd18)) begin
                timer();
                if (phase == P_C2) port_logic();
            end
            if (at(5'd19)) next_slot();
            if (at(5'd6) && phase == P_C2 && (ir == 8'h09 || ir == 8'h0A))
                a <= ir[0] ? p1_pins : p2_pins;                 // IN A,P: S2 of cycle 2
            if (at(5'd24) && phase == P_C1) port_early();
            if (at(5'd24) && phase == P_ONE) timer_control();
            if (at(5'd0)) phase <= next_phase;                  // a new cycle
        end
    end

    // the byte a slot brought, as its strobe rises (h 9)
    task automatic byte_in();
        case (slot)
            S_OPCODE: begin
                ir <= bus_pins;
                pc <= step(slot_addr);
                phase <= two_cycles(bus_pins) ? P_C1 : P_ONE;
            end
            S_IMM: begin
                imm <= bus_pins;
                imm_page <= slot_addr[11:8];
                pc <= step(slot_addr);
            end
            S_TABLE, S_READ: imm <= bus_pins;
            S_PROG: if (order == 2'd0) imm <= {4'd0, p2_pins[3:0]};
            default: ;
        endcase
        if (slot == S_OPCODE || slot == S_TABLE || slot == S_READ || slot == S_WRITE)
            bus_latched <= 1'b0;                                // the BUS used: OUTL's value gone (3.7)
    endtask

    // the slot of the cycle to come (decided in S4): the next opcode, an
    // interrupt's CALL, or what a 2-cycle instruction's first cycle needs
    task automatic next_slot();
        slot <= S_IDLE;
        slot_bus <= 1'b1;
        case (phase)
            P_C1: begin
                next_phase <= P_C2;
                case (first_slot(ir))
                    SLOT_IMM: begin
                        slot <= S_IMM;
                        slot_addr <= pc;
                    end
                    SLOT_TABLE: begin
                        slot <= S_TABLE;
                        slot_addr <= ir == 8'hE3 ? {4'b0011, a} : {pc[11:8], a};
                    end
                    default:
                        if (expander(ir[7:2]))
                            slot <= S_PROG;
                        else if (ir == 8'h80 || ir == 8'h81 || ir == 8'h08) begin
                            slot <= S_READ;                     // MOVX A,@R; INS A,BUS
                            slot_bus <= ir != 8'h08;
                            slot_addr <= {4'd0, ram[bs ? 6'd24 + 6'(ir[0]) : 6'(ir[0])]};
                        end else if (ir == 8'h90 || ir == 8'h91) begin
                            slot <= S_WRITE;                    // MOVX @R,A
                            slot_addr <= {4'd0, ram[bs ? 6'd24 + 6'(ir[0]) : 6'(ir[0])]};
                        end
                endcase
            end
            P_V1: next_phase <= P_V2;
            default:                                            // the instruction ends: what next
                if (!in_int && phase != P_V2 && ((ie && int_low) || (tie && t_req))) begin
                    next_phase <= P_V1;
                    vector <= ie && int_low ? 12'h003 : 12'h007;    // the external first (2.1)
                end else begin
                    next_phase <= P_IDLE;
                    slot <= S_OPCODE;
                    slot_addr <= {in_int ? 1'b0 : pc[11], pc[10:0]};
                end
        endcase
    endtask

    // the first cycle's S3 of a 2-cycle instruction: a jump's condition,
    // the 8243's order
    task automatic prepare();
        cond <= jump_taken(ir);
        if (ir == 8'h16 && tf)
            tf <= 1'b0;                                         // JTF: tested, reset (p. 4-21)
        if (expander(ir[7:2]))
            order <= ir[7] ? {1'b1, ir[4]} : {1'b0, ir[5]};
    endtask

    // S5 of the first cycle: OUTL's output, ANL/ORL's port read; the timer's
    // and counter's start and stop (1-cycle, S5 of their cycle)
    task automatic port_early();
        case (ir)
            8'h39: port1 <= a;
            8'h3A: port2 <= a;
            8'h02: begin
                bus_latch <= a;
                bus_latched <= 1'b1;
            end
            8'h89, 8'h99: held <= p1_pins;
            8'h8A, 8'h9A: held <= p2_pins;
            8'h88, 8'h98: held <= bus_latch;
            default: ;
        endcase
    endtask

    task automatic timer_control();
        case (ir)
            8'h55: begin
                tmode <= 2'd1;
                prescale <= '0;
            end
            8'h45: tmode <= 2'd2;
            8'h65: tmode <= 2'd0;
            default: ;
        endcase
    endtask

    // S4 of the second cycle: ANL/ORL's result out
    task automatic port_logic();
        case (ir)
            8'h89: port1 <= held | imm;
            8'h99: port1 <= held & imm;
            8'h8A: port2 <= held | imm;
            8'h9A: port2 <= held & imm;
            8'h88: bus_latch <= held | imm;
            8'h98: bus_latch <= held & imm;
            default: ;
        endcase
    endtask

    // S4 of every cycle: the timer's prescaler (32 cycles a count) or the
    // counter on T1's falling edges; an overflow sets TF and asks
    task automatic timer();
        logic count;
        t1_was <= p39;
        count = tmode == 2'd1 ? prescale == 5'd31 : tmode == 2'd2 && t1_was && !p39;
        if (tmode == 2'd1) prescale <= prescale + 5'd1;
        if (count) begin
            t <= t + 8'd1;
            if (t == 8'hFF) begin
                tf <= 1'b1;
                if (tie) t_req <= 1'b1;
            end
        end
    endtask

    // an interrupt's inserted CALL, in S3 of its second cycle
    task automatic take_vector();
        ram[stack_at(sp)] <= pc[7:0];
        ram[stack_at(sp) + 6'd1] <= {cy, ac, f0, bs, pc[11:8]};
        sp <= sp + 3'd1;
        pc <= vector;
        in_int <= 1'b1;
        if (vector == 12'h007) t_req <= 1'b0;
    endtask

    // ---- the instructions' effects, in S3 of their last cycle (4.1) ----------
    task automatic execute(logic second);
        if (second) execute_two();
        else execute_one();
    endtask

    task automatic execute_one();
        acc_t r;
        logic [7:0] x;
        x = ir[3] ? ram[r_at] : ram[ri_at];
        casez (ir)
            8'b0110_1???, 8'b0111_1???, 8'b0101_1???, 8'b0100_1???, 8'b1101_1???,
            8'b0110_000?, 8'b0111_000?, 8'b0101_000?, 8'b0100_000?, 8'b1101_000?: begin
                r = accumulate(ir[7:4], x);
                a <= r.a; cy <= r.cy; ac <= r.ac;
            end
            8'h27, 8'h37, 8'h17, 8'h07, 8'h57, 8'h47, 8'hE7, 8'hF7, 8'h77, 8'h67, 8'h97, 8'hA7: begin
                r = unary(ir);
                a <= r.a; cy <= r.cy; ac <= r.ac;
            end
            8'b0001_1???: ram[r_at] <= ram[r_at] + 8'd1;                 // INC Rr
            8'b0001_000?: ram[ri_at] <= ram[ri_at] + 8'd1;               // INC @R
            8'b1100_1???: ram[r_at] <= ram[r_at] - 8'd1;                 // DEC Rr
            8'b0010_1???: begin a <= ram[r_at]; ram[r_at] <= a; end      // XCH A,Rr
            8'b0010_000?: begin a <= ram[ri_at]; ram[ri_at] <= a; end    // XCH A,@R
            8'b0011_000?: begin                                          // XCHD A,@R
                a <= {a[7:4], ram[ri_at][3:0]};
                ram[ri_at] <= {ram[ri_at][7:4], a[3:0]};
            end
            8'b1111_1???: a <= ram[r_at];                                // MOV A,Rr
            8'b1111_000?: a <= ram[ri_at];                               // MOV A,@R
            8'b1010_1???: ram[r_at] <= a;                                // MOV Rr,A
            8'b1010_000?: ram[ri_at] <= a;                               // MOV @R,A
            default: control_one();
        endcase
    endtask

    // the 1-cycle instructions that do not touch A's arithmetic
    task automatic control_one();
        case (ir)
            8'hC7: a <= psw;                                             // MOV A,PSW
            8'hD7: begin                                                 // MOV PSW,A
                {cy, ac, f0, bs} <= a[7:4];
                sp <= a[2:0];
            end
            8'h42: a <= t;
            8'h62: t <= a;
            8'h05: ie <= 1'b1;
            8'h15: ie <= 1'b0;
            8'h25: tie <= 1'b1;
            8'h35: begin                                                 // DIS TCNTI: a pending request cleared
                tie <= 1'b0;
                t_req <= 1'b0;
            end
            8'hC5: bs <= 1'b0;
            8'hD5: bs <= 1'b1;
            8'hE5: dbf <= 1'b0;
            8'hF5: dbf <= 1'b1;
            8'h85: f0 <= 1'b0;
            8'h95: f0 <= !f0;
            8'hA5: f1 <= 1'b0;
            8'hB5: f1 <= !f1;
            default: ;                                                   // NOP, ENT0 CLK, the timer's (S5), undefined
        endcase
    endtask

    task automatic execute_two();
        acc_t r;
        logic [11:0] target;
        target = {in_int ? 1'b0 : dbf, ir[7:5], imm};
        casez (ir)
            8'h03, 8'h13, 8'h43, 8'h53, 8'hD3: begin
                r = accumulate_imm(ir, imm);
                a <= r.a; cy <= r.cy; ac <= r.ac;
            end
            8'h23: a <= imm;                                             // MOV A,#
            8'b1011_1???: ram[r_at] <= imm;                              // MOV Rr,#
            8'b1011_000?: ram[ri_at] <= imm;                             // MOV @R,#
            8'b???0_0100: pc <= target;                                  // JMP
            8'b???1_0100: begin                                          // CALL
                ram[stack_at(sp)] <= pc[7:0];
                ram[stack_at(sp) + 6'd1] <= {cy, ac, f0, bs, pc[11:8]};
                sp <= sp + 3'd1;
                pc <= target;
            end
            8'b1110_1???: begin                                          // DJNZ
                ram[r_at] <= ram[r_at] - 8'd1;
                if (ram[r_at] != 8'd1) pc <= {imm_page, imm};
            end
            8'b???1_0010, 8'h16, 8'h26, 8'h36, 8'h46, 8'h56, 8'h76, 8'h86, 8'h96,
            8'hB6, 8'hC6, 8'hE6, 8'hF6:
                if (cond) pc <= {imm_page, imm};
            8'h83, 8'h93: begin                                          // RET, RETR
                pc <= {ram[stack_at(sp - 3'd1) + 6'd1][3:0], ram[stack_at(sp - 3'd1)]};
                sp <= sp - 3'd1;
                if (ir == 8'h93) begin
                    {cy, ac, f0, bs} <= ram[stack_at(sp - 3'd1) + 6'd1][7:4];
                    in_int <= 1'b0;
                end
            end
            8'hA3, 8'hE3, 8'h08, 8'h80, 8'h81: a <= imm;                // MOVP, MOVP3, INS, MOVX A
            8'hB3: pc <= {pc[11:8], imm};                               // JMPP @A
            8'b0000_11??: a <= imm;                                      // MOVD A,P (its nibble)
            default: ;                                                   // the ports' (S5, S4), MOVD to, MOVX to
        endcase
    endtask

    // ---- the pins ---------------------------------------------------------------
    logic fetching, strobe;
    assign fetching = slot == S_OPCODE || slot == S_IMM || slot == S_TABLE;
    assign strobe = h >= 5'd27 || h <= 5'd8;                    // S5 into S2: PSEN, RD, WR (p. 2-10)
    assign p11 = h >= 5'd17 && h <= 5'd23;                      // ALE
    assign p9 = !(fetching && strobe);                          // PSEN
    assign p8 = !(slot == S_READ && strobe);                    // RD
    assign p10 = !(slot == S_WRITE && strobe);                  // WR
    assign p25 = !(slot == S_PROG && (h >= 5'd24 || h <= 5'd8)); // PROG

    // BUS: the slot's address around ALE's fall, MOVX's data, OUTL's latch
    logic       bus_addr, bus_data;
    logic [7:0] bus_q;
    assign bus_addr = slot != S_IDLE && slot != S_PROG && slot_bus && h >= 5'd21 && h <= 5'd25;
    assign bus_data = slot == S_WRITE && (h >= 5'd26 || h <= 5'd9);
    assign bus_q = bus_addr ? slot_addr[7:0] : bus_data ? a : bus_latch;
    assign {p19, p18, p17, p16, p15, p14, p13, p12} = bus_q;
    assign {p19_oe, p18_oe, p17_oe, p16_oe, p15_oe, p14_oe, p13_oe, p12_oe} =
        {8{bus_addr || bus_data || (bus_latched && slot == S_IDLE)}};

    // P20..P23: the port, PC11..PC8 around a fetch, the 8243's order and data
    logic       exp_order, exp_data, exp_read;
    logic [3:0] p2_low;
    assign exp_order = phase == P_C1 && expander(ir[7:2]) && h >= 5'd12 && h <= 5'd25;
    assign exp_data = slot == S_PROG && (h >= 5'd26 || h <= 5'd9);
    assign exp_read = exp_data && order == 2'd0;
    assign p2_low = exp_order ? {ir[7] ? {1'b1, ir[4]} : {1'b0, ir[5]}, ir[1:0]}
                  : exp_data ? a[3:0]
                  : fetching && (h >= 5'd19 || h <= 5'd8) ? slot_addr[11:8]
                  : port2[3:0];
    assign {p24, p23, p22, p21} = p2_low;
    assign {p24_oe, p23_oe, p22_oe, p21_oe} = {4{!exp_read}};
    assign {p38, p37, p36, p35} = port2[7:4];
    assign {p38_oe, p37_oe, p36_oe, p35_oe} = 4'hF;
    assign {p34, p33, p32, p31, p30, p29, p28, p27} = port1;
    assign {p34_oe, p33_oe, p32_oe, p31_oe, p30_oe, p29_oe, p28_oe, p27_oe} = 8'hFF;
endmodule


