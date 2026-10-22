// КР1807ВМ1: each instruction as the steps it takes on the bus, one
// transaction a step, as docs/t11-model.md builds them from the T-11
// Engineering Specification (App. B microcycles, App. E transactions) and
// the User's Guide (Table A-3 sequences).  16-bit bus.
//
// Every value here is at most 64 bits wide: Verilator keeps a wider one as
// an array it clears and copies at every call, and the core's functions are
// laid out at every tick - that once cost the board model half its time.
package kr1807vm1_steps_pkg;
    import kr1807vm1_pkg::*;

    // what an instruction's U_EXEC step does
    typedef enum logic [3:0] {
        K_ALU, K_BR, K_SOB, K_JMP, K_JSR, K_RTS, K_CCC, K_MFPT, K_MTPS,
        K_HALT, K_RTI, K_RTT, K_WAIT, K_NONE, K_TRAP, K_RESET
    } kind_t;

    // up to 12 steps (the longest, an interrupt with an external vector,
    // takes 11): 64 bits.  The steps are kept as their codes: Quartus takes
    // no packed array of an enum.
    typedef struct packed {
        logic [11:0][4:0] ops;     // step_t is 5 bits
        logic  [3:0]  steps;       // how many (named apart from the core's len: Quartus mixes them up)
    } prog_t;

    typedef struct packed {
        kind_t       kind;
        alu_op_t     aop;
        logic        bw;            // a byte operation
        logic        writes;        // the destination is written
        logic [2:0]  smode, sreg, dmode, dreg;
        logic [7:0]  vec;           // a trap's vector
        logic [7:0]  rep;           // U_REP repeats rep + 1 times
    } decoded_t;

    // between instructions: the next fetch, or a service
    typedef enum logic [2:0] { S_FETCH, S_HALT, S_PF, S_TRACE, S_INT } serve_t;

    function automatic prog_t add(input prog_t p, input step_t s);
        p.ops[p.steps] = s;
        p.steps = p.steps + 4'd1;
        return p;
    endfunction

    function automatic prog_t add2(input prog_t p, input step_t a, input step_t b);
        return add(add(p, a), b);
    endfunction

    // a trap: N N W Ref N W R R N (UG Table A-3, EMT)
    function automatic prog_t trap_steps();
        prog_t p = '0;
        p = add2(p, U_N, U_N);
        p = add2(p, U_PUSH_PS, U_N);
        p = add2(p, U_N, U_PUSH_PC);
        p = add2(p, U_VEC_PC, U_VEC_PS);
        return add(p, U_N);
    endfunction

    // HALT: N N W Ref N W N A N - App. B's 14 microcycles (Table A-3 has an
    // N more); the last step loads PC and PS
    function automatic prog_t halt_steps();
        prog_t p = '0;
        p = add2(p, U_N, U_N);
        p = add2(p, U_PUSH_PS, U_N);
        p = add2(p, U_N, U_PUSH_PC);
        p = add2(p, U_N, U_ASPI);
        return add(p, U_EXEC);
    endfunction

    // between instructions; an interrupt is I N [N] N W Ref N W R R N, the
    // N in brackets only for an external vector (UG Table A-3)
    function automatic prog_t service(input serve_t s, input logic external);
        prog_t p = '0;
        case (s)
            S_FETCH: p = add2(p, U_FETCH, U_DECODE);
            S_HALT:  p = halt_steps();
            S_INT: begin
                p = add2(p, U_IACK, U_N);
                if (external) p = add(p, U_N);
                p = add2(p, U_N, U_PUSH_PS);
                p = add2(p, U_N, U_N);
                p = add2(p, U_PUSH_PC, U_VEC_PC);
                p = add2(p, U_VEC_PS, U_N);
            end
            default: p = trap_steps();
        endcase
        return p;
    endfunction

    // the first step of service(s, ...), without building the rest
    function automatic step_t service_first(input serve_t s);
        case (s)
            S_FETCH: return U_FETCH;
            S_INT:   return U_IACK;
            default: return U_N;
        endcase
    endfunction

    // an operand's address: (R)+ and (R) need no step of their own, @(R)+
    // reads the pointer, -(R) a NOP, @-(R) both, X(R) the index word and a
    // NOP, @X(R) those and the pointer
    function automatic prog_t address(input prog_t p, input logic [2:0] mode, input logic s);
        case (mode)
            3'd3: p = add(p, s ? U_PTR_S : U_PTR_D);
            3'd4: p = add(p, s ? U_SUM_S : U_SUM_D);
            3'd5: p = add2(p, s ? U_SUM_S : U_SUM_D, s ? U_PTR_S : U_PTR_D);
            3'd6: p = add2(p, s ? U_IDX_S : U_IDX_D, s ? U_SUM_S : U_SUM_D);
            3'd7: begin
                p = add2(p, s ? U_IDX_S : U_IDX_D, s ? U_SUM_S : U_SUM_D);
                p = add(p, s ? U_PTR_S : U_PTR_D);
            end
            default: ;
        endcase
        return p;
    endfunction

    // the source: its address, then the read (a register is read when used)
    function automatic prog_t source(input prog_t p, input logic [2:0] mode);
        p = address(p, mode, 1'b1);
        return mode == 0 ? p : add(p, U_RD_S);
    endfunction

    // the destination: a register in one NOP; memory read and written back
    // (DATI then DATO, MOV too), or read with the flags set in a NOP
    function automatic prog_t destination(input prog_t p, input logic [2:0] mode, input logic w);
        p = address(p, mode, 1'b0);
        if (mode == 0)
            return add(p, U_EXEC);
        return add2(p, U_RD_D, w ? U_WR_D : U_EXEC);
    endfunction

    // JMP and JSR take the address, never the operand
    function automatic prog_t jump(input prog_t p, input logic [2:0] mode);
        p = address(p, mode, 1'b0);
        if (mode == 1 || mode == 2) p = add(p, U_EA_D);
        if (mode == 2 || mode == 4) p = add(p, U_N);
        return p;
    endfunction

    // RESET: 4 N, -BCLR for rep + 1 NOPs, N, then an ASPI and an N
    function automatic prog_t reset_steps();
        prog_t p = '0;
        p = add2(add2(p, U_N, U_N), U_N, U_N);
        p = add2(add2(p, U_BCLR_ON, U_REP), U_BCLR_OFF, U_N);
        return add2(p, U_ASPI, U_N);
    endfunction

    // the steps of a decoded instruction (an ALU operation without a source
    // has smode 0, which adds none)
    function automatic prog_t steps(input kind_t kind, input logic writes,
                                    input logic [2:0] smode, input logic [2:0] dmode);
        prog_t p = '0;
        case (kind)
            K_ALU:   p = destination(source(p, smode), dmode, writes);
            K_MTPS:  p = add(add2(add2(source(p, smode), U_N, U_N), U_N, U_N), U_EXEC);
            K_HALT:  p = halt_steps();
            K_WAIT:  p = add2(add2(p, U_N, U_N), U_N, U_WAIT);
            K_RTI, K_RTT: p = add(add2(p, U_POP_PC, U_POP_PS), U_EXEC);
            K_MFPT:  p = add2(p, U_N, U_EXEC);
            K_JMP:   p = add(jump(p, dmode), U_EXEC);
            K_RTS:   p = add(add2(p, U_EXEC, U_POP_R), U_N);
            K_CCC, K_SOB: p = add(add2(p, U_N, U_N), U_EXEC);
            K_BR:    p = add(p, U_EXEC);
            K_JSR:   p = add2(add2(jump(p, dmode), U_N, U_PUSH_T), U_N, U_EXEC);
            K_RESET: p = reset_steps();
            default: p = trap_steps();
        endcase
        return p;
    endfunction

    function automatic decoded_t trap(input decoded_t d, input logic [7:0] v);
        d.kind = K_TRAP;
        d.vec  = v;
        return d;
    endfunction

    function automatic decoded_t double_operand(input decoded_t d, input logic [3:0] op);
        d.kind = K_ALU;
        case (op[2:0])
            3'd1: d.aop = A_MOV;
            3'd2: d.aop = A_CMP;
            3'd3: d.aop = A_BIT;
            3'd4: d.aop = A_BIC;
            3'd5: d.aop = A_BIS;
            default: d.aop = op[3] ? A_SUB : A_ADD;
        endcase
        d.bw     = op[3] && op[2:0] != 3'd6;
        d.writes = d.aop != A_CMP && d.aop != A_BIT;
        return d;
    endfunction

    // CLR..TST, ROR..ASL, SXT, MFPS, MTPS and the reserved ones among them
    function automatic decoded_t single_operand(input decoded_t d, input logic b, input logic [3:0] f);
        d.kind = K_ALU;
        d.bw   = b;
        d.smode = 3'd0;
        case (f)
            4'o00: d.aop = A_CLR;  4'o01: d.aop = A_COM;  4'o02: d.aop = A_INC;
            4'o03: d.aop = A_DEC;  4'o04: d.aop = A_NEG;  4'o05: d.aop = A_ADC;
            4'o06: d.aop = A_SBC;  4'o07: d.aop = A_TST;  4'o10: d.aop = A_ROR;
            4'o11: d.aop = A_ROL;  4'o12: d.aop = A_ASR;  4'o13: d.aop = A_ASL;
            4'o17: d.aop = b ? A_MFPS : A_SXT;
            4'o14: begin
                if (!b) return trap(d, 8'o10);              // MARK
                d.kind  = K_MTPS;
                d.smode = d.dmode;
                d.sreg  = d.dreg;
                return d;
            end
            default: return trap(d, 8'o10);                 // MFPI MTPI MFPD MTPD
        endcase
        if (d.aop == A_SXT) d.bw = 1'b0;
        d.writes = d.aop != A_TST;
        return d;
    endfunction

    // 000000-000377: HALT .. MFPT, JMP, RTS, CCC/SCC, SWAB, and BR
    function automatic decoded_t group_zero(input decoded_t d, input logic [8:0] ir);
        case (ir[8:6])
            3'd0:
                case (ir[5:0])
                    6'o00: d.kind = K_HALT;
                    6'o01: d.kind = K_WAIT;
                    6'o02: d.kind = K_RTI;
                    6'o03: d = trap(d, 8'o14);
                    6'o04: d = trap(d, 8'o20);
                    6'o05: begin d.kind = K_RESET; d.rep = 8'd26; end
                    6'o06: d.kind = K_RTT;
                    6'o07: d.kind = K_MFPT;
                    default: d = trap(d, 8'o10);
                endcase
            3'd1:
                if (ir[5:3] == 0) d = trap(d, 8'o10);       // JMP R traps
                else d.kind = K_JMP;
            3'd2:
                case (ir[5:3])
                    3'd0: d.kind = K_RTS;
                    3'd4, 3'd5, 3'd6, 3'd7: d.kind = K_CCC;
                    default: d = trap(d, 8'o10);                // 000210-000237, SPL
                endcase
            3'd3: begin d.kind = K_ALU; d.aop = A_SWAB; d.writes = 1'b1; end
            default: d.kind = K_BR;
        endcase
        return d;
    endfunction

    function automatic decoded_t decode(input logic [15:0] ir);
        decoded_t d = '0;
        d.kind  = K_NONE;
        d.smode = ir[11:9];
        d.sreg  = ir[8:6];
        d.dmode = ir[5:3];
        d.dreg  = ir[2:0];
        if (ir[14:12] != 3'd0 && ir[14:12] != 3'd7)
            return double_operand(d, ir[15:12]);
        if (ir[15:12] == 4'o17)
            return trap(d, 8'o10);                          // FPP
        if (ir[15:12] == 4'd7) begin
            case (ir[11:9])
                3'd4: begin                                 // XOR
                    d.kind = K_ALU; d.aop = A_XOR; d.writes = 1'b1; d.smode = 3'd0;
                end
                3'd7: d.kind = K_SOB;
                default: d = trap(d, 8'o10);                // MUL DIV ASH ASHC FIS
            endcase
            return d;
        end
        // 000000-007777 and 100000-107777
        case (ir[11:9])
            3'd0: begin
                if (ir[15]) d.kind = K_BR;
                else d = group_zero(d, ir[8:0]);
                return d;
            end
            3'd1, 3'd2, 3'd3: begin d.kind = K_BR; return d; end
            3'd4:
                if (ir[15]) return trap(d, ir[8] ? 8'o34 : 8'o30);  // EMT, TRAP
                else if (ir[5:3] == 0) return trap(d, 8'o10);       // JSR R traps
                else begin d.kind = K_JSR; return d; end
            3'd5, 3'd6: return single_operand(d, ir[15], {!ir[9], ir[8:6]});  // 005x: 0x, 006x: 1x
            default: return trap(d, 8'o10);                 // 007000-007777, 107000-107777
        endcase
    endfunction
endpackage
