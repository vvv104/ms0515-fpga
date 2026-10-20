// What the parts of the КР1807ВМ1 model share: the bus transactions, the
// microcycles an instruction is made of, and the arithmetic.
package kr1807vm1_pkg;
    // bus transactions (docs/t11-model.md)
    localparam logic [2:0] BUS_NOP   = 3'd0;   // 3T, no strobes
    localparam logic [2:0] BUS_READ  = 3'd1;   // 6T
    localparam logic [2:0] BUS_WRITE = 3'd2;   // 6T
    localparam logic [2:0] BUS_ASPI  = 3'd3;   // 4T: -CAS and PI, requests sampled
    localparam logic [2:0] BUS_IACK  = 3'd4;   // 4T: SEL1, -RAS, vector read
    localparam logic [2:0] BUS_DMG   = 3'd5;   // 8T: a DMA grant, SEL 11, DAL let go

    // The steps of an instruction, one bus transaction each (the core's
    // `step`): what it puts on the bus and what it does when it is done.
    typedef enum logic [4:0] {
        U_FETCH,        // read at PC (SEL0): IR, PC += 2
        U_DECODE,       // bus NOP: the instruction's steps chosen
        U_N,            // bus NOP: nothing of its own
        U_IDX_S,        // read at PC: the index word of the source, PC += 2
        U_SUM_S,        // bus NOP: -(R) / @-(R) decrement, or X(R) sum
        U_PTR_S,        // read the source's pointer
        U_RD_S,         // read the source
        U_IDX_D, U_SUM_D, U_PTR_D,
        U_RD_D,         // read the destination (and work out a result to write)
        U_WR_D,         // write the result back
        U_EXEC,         // bus NOP: the instruction's own effect
        U_EA_D,         // bus NOP: JMP/JSR (R), (R)+ address
        U_PUSH_PS, U_PUSH_PC, U_PUSH_T,
        U_VEC_PC, U_VEC_PS,
        U_POP_PC, U_POP_PS, U_POP_R,
        U_ASPI,         // ASPI
        U_IACK,         // IACK for the request being served
        U_WAIT,         // ASPI of the WAIT loop: back to its start while nothing asks
        U_BCLR_ON, U_BCLR_OFF,  // bus NOP asserting / negating -BCLR
        U_REP,          // bus NOP, repeated `rep` + 1 times
        U_START         // ASPI that ends power-up: SP, PC, PS loaded
    } step_t;

    // ALU operations
    typedef enum logic [4:0] {
        A_MOV, A_CMP, A_BIT, A_BIC, A_BIS, A_ADD, A_SUB, A_XOR,
        A_CLR, A_COM, A_INC, A_DEC, A_NEG, A_ADC, A_SBC, A_TST,
        A_ROR, A_ROL, A_ASR, A_ASL, A_SWAB, A_SXT, A_MFPS
    } alu_op_t;

    typedef struct packed {
        logic [15:0] res;
        logic [3:0]  nzvc;
    } alu_t;

    // One ALU operation on src and dst; the byte forms on the low bytes.
    // n_in and c_in are N and C before.
    function automatic alu_t alu(input alu_op_t op, input logic bw, input logic [15:0] s,
                                 input logic [15:0] d, input logic n_in, input logic c_in);
        logic [15:0] mask, a, b, r;
        logic [16:0] wide;
        logic sa, sb, sr, n, z, v, c;
        mask = bw ? 16'h00FF : 16'hFFFF;
        a = s & mask;
        b = d & mask;
        sa = bw ? a[7] : a[15];
        sb = bw ? b[7] : b[15];
        v = 1'b0;
        c = c_in;
        wide = '0;
        case (op)
            A_MOV, A_MFPS: r = a;
            A_CMP: begin wide = {1'b0, a} - {1'b0, b}; r = wide[15:0]; c = a < b; end
            A_BIT: r = a & b;
            A_BIC: r = ~a & b;
            A_BIS: r = a | b;
            A_ADD: begin wide = {1'b0, a} + {1'b0, b}; r = wide[15:0]; c = wide[16]; end
            A_SUB: begin wide = {1'b0, b} - {1'b0, a}; r = wide[15:0]; c = b < a; end
            A_XOR: r = a ^ b;
            A_CLR: begin r = '0; c = 1'b0; end
            A_COM: begin r = ~b; c = 1'b1; end
            A_INC: r = b + 16'd1;
            A_DEC: r = b - 16'd1;
            A_NEG: r = '0 - b;
            A_ADC: r = b + {15'd0, c_in};
            A_SBC: r = b - {15'd0, c_in};
            A_TST: begin r = b; c = 1'b0; end
            A_ROR: begin r = bw ? {8'd0, c_in, b[7:1]} : {c_in, b[15:1]}; c = b[0]; end
            A_ROL: begin r = {b[14:0], c_in}; c = sb; end
            A_ASR: begin r = bw ? {8'd0, b[7], b[7:1]} : {b[15], b[15:1]}; c = b[0]; end
            A_ASL: begin r = {b[14:0], 1'b0}; c = sb; end
            A_SWAB: begin r = {b[7:0], b[15:8]}; c = 1'b0; end
            A_SXT: r = n_in ? 16'hFFFF : 16'h0000;
            default: r = b;
        endcase
        r = r & (op == A_SWAB || op == A_SXT ? 16'hFFFF : mask);
        sr = bw ? r[7] : r[15];
        n = sr;
        z = (r & mask) == 0;
        case (op)
            A_CMP: v = (sa != sb) && (sr == sb);
            A_ADD: v = (sa == sb) && (sr != sa);
            A_SUB: v = (sa != sb) && (sr == sa);
            A_INC: v = !sb && sr;
            A_DEC: v = sb && !sr;
            A_NEG: begin v = sr && (r & mask) == (bw ? 16'h0080 : 16'h8000); c = !z; end
            A_ADC: begin v = c_in && !sb && sr; c = c_in && b == mask; end
            A_SBC: begin v = b == (bw ? 16'h0080 : 16'h8000); c = c_in && b == 0; end
            A_ROR, A_ROL, A_ASR, A_ASL: v = n ^ c;
            A_SWAB: begin n = r[7]; z = r[7:0] == 0; end
            A_SXT: begin n = n_in; z = !n_in; end
            default: ;
        endcase
        return '{res: r, nzvc: {n, z, v, c}};
    endfunction
endpackage
