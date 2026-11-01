// КМ1816ВЕ48 (8048/8035): the instruction set's properties and arithmetic,
// for km1816ve48.sv.  Sources: the MCS-48 Family User's Manual (1980) -
// the instruction summary (p. 4-5) for bytes and cycles, the descriptions
// (4.1) for what each does; tools/mcs48_opcodes.py holds the same table,
// each opcode checked against a source (docs/km1816ve48.md).
package km1816ve48_pkg;

    // what the fetch slot of an instruction's first cycle brings
    typedef enum logic [1:0] {
        SLOT_NEXT,      // a 1-cycle instruction: the next opcode
        SLOT_IMM,       // the second byte: data or a jump's target
        SLOT_TABLE,     // a program memory read (MOVP, MOVP3, JMPP): at A
        SLOT_NONE       // nothing fetched: the slot is the bus's or PROG's
    } slot_t;

    function automatic logic two_cycles(logic [7:0] op);
        casez (op)
            8'h02, 8'h08, 8'h09, 8'h0A, 8'h39, 8'h3A,                 // OUTL BUS, INS, IN, OUTL
            8'b0000_11??, 8'b0011_11??, 8'b1000_11??, 8'b1001_11??,   // MOVD, ORLD, ANLD
            8'h80, 8'h81, 8'h90, 8'h91,                               // MOVX
            8'h83, 8'h93, 8'hA3, 8'hB3, 8'hE3:                        // RET RETR MOVP JMPP MOVP3
                return 1'b1;
            default:
                return has_imm(op);
        endcase
    endfunction

    // a second byte: immediate data or a jump's target
    function automatic logic has_imm(logic [7:0] op);
        casez (op)
            8'h03, 8'h13, 8'h23, 8'h43, 8'h53, 8'hD3,                 // ADD ADDC MOV ORL ANL XRL A,#
            8'h88, 8'h89, 8'h8A, 8'h98, 8'h99, 8'h9A,                 // ORL/ANL BUS P1 P2,#
            8'hB0, 8'hB1, 8'b1011_1???,                               // MOV @R,#  MOV R,#
            8'b???0_0100, 8'b???1_0100, 8'b???1_0010,                 // JMP CALL JBb
            8'h16, 8'h26, 8'h36, 8'h46, 8'h56, 8'h76, 8'h86, 8'h96,   // JTF JNT0 JT0 JNT1 JT1 JF1 JNI JNZ
            8'hB6, 8'hC6, 8'hE6, 8'hF6, 8'b1110_1???:                 // JF0 JZ JNC JC DJNZ
                return 1'b1;
            default:
                return 1'b0;
        endcase
    endfunction

    function automatic slot_t first_slot(logic [7:0] op);
        if (!two_cycles(op)) return SLOT_NEXT;
        if (has_imm(op)) return SLOT_IMM;
        if (op == 8'hA3 || op == 8'hB3 || op == 8'hE3) return SLOT_TABLE;
        return SLOT_NONE;
    endfunction

    // the instructions that use the 8243 through P2 lower and PROG
    // op: the opcode's bits 7..2 (1..0 name the port)
    function automatic logic expander(logic [5:0] op);
        return op == 6'b0000_11 || op == 6'b0011_11 || op == 6'b1000_11 || op == 6'b1001_11;
    endfunction

    // ADD and ADDC: the sum, the carry out of bit 7 and of bit 3
    typedef struct packed {
        logic [7:0] a;
        logic       cy;
        logic       ac;
    } sum_t;

    function automatic sum_t add(logic [7:0] a, logic [7:0] b, logic c);
        sum_t s;
        logic [8:0] full = {1'b0, a} + {1'b0, b} + {8'd0, c};
        s.a = full[7:0];
        s.cy = full[8];
        s.ac = {1'b0, a[3:0]} + {1'b0, b[3:0]} + {4'd0, c} > 5'd15;
        return s;
    endfunction

    // DA A (p. 4-14): six added to the low digit if over nine or AC, then
    // to the high digit if over nine or C; an overflow sets C
    // the result {C, A}
    function automatic logic [8:0] decimal(logic [7:0] a, logic cy, logic ac);
        logic [8:0] t = {1'b0, a};
        logic       c = cy;
        if (a[3:0] > 4'd9 || ac) begin
            t = t + 9'h006;
            if (t[8]) c = 1'b1;
        end
        if (t[7:4] > 4'd9 || c) begin
            t = {1'b0, t[7:0]} + 9'h060;
            if (t[8]) c = 1'b1;
        end
        return {c, t[7:0]};
    endfunction
endpackage
