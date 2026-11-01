#!/usr/bin/env python3
"""The MCS-48's opcodes (8048/8035), for checking the КМ1816ВЕ48 model's
decoder against sources: the MCS-48 User's Manual (1980) - its 8022
instruction summary (p. 4-7) gives the hex codes the 8022 shares with the
8048, the instruction descriptions (4.1) the rest - and the МС 7004
firmware's listing, whose every line carries its bytes and mnemonic.

    python tools/mcs48_opcodes.py LISTING   - the listing's opcodes against
                                              this table (exit 1 on a mismatch)
"""
import re
import sys

R = [f"R{i}" for i in range(8)]


def table():
    """opcode -> (mnemonic, bytes, cycles)."""
    t = {}

    def put(op, mn, n, c):
        t[op] = (mn, n, c)

    for i in range(8):
        put(0x68 + i, f"ADD A,{R[i]}", 1, 1)
        put(0x78 + i, f"ADDC A,{R[i]}", 1, 1)
        put(0x58 + i, f"ANL A,{R[i]}", 1, 1)
        put(0x48 + i, f"ORL A,{R[i]}", 1, 1)
        put(0xD8 + i, f"XRL A,{R[i]}", 1, 1)
        put(0x18 + i, f"INC {R[i]}", 1, 1)
        put(0xC8 + i, f"DEC {R[i]}", 1, 1)
        put(0x28 + i, f"XCH A,{R[i]}", 1, 1)
        put(0xF8 + i, f"MOV A,{R[i]}", 1, 1)
        put(0xA8 + i, f"MOV {R[i]},A", 1, 1)
        put(0xB8 + i, f"MOV {R[i]},#", 2, 2)
        put(0xE8 + i, f"DJNZ {R[i]},", 2, 2)
        put(0x04 + 0x20 * i, "JMP", 2, 2)
        put(0x14 + 0x20 * i, "CALL", 2, 2)
        put(0x12 + 0x20 * i, f"JB{i}", 2, 2)
    for i in range(2):
        r = f"@R{i}"
        put(0x60 + i, f"ADD A,{r}", 1, 1)
        put(0x70 + i, f"ADDC A,{r}", 1, 1)
        put(0x50 + i, f"ANL A,{r}", 1, 1)
        put(0x40 + i, f"ORL A,{r}", 1, 1)
        put(0xD0 + i, f"XRL A,{r}", 1, 1)
        put(0x10 + i, f"INC {r}", 1, 1)
        put(0x20 + i, f"XCH A,{r}", 1, 1)
        put(0x30 + i, f"XCHD A,{r}", 1, 1)
        put(0xF0 + i, f"MOV A,{r}", 1, 1)
        put(0xA0 + i, f"MOV {r},A", 1, 1)
        put(0xB0 + i, f"MOV {r},#", 2, 2)
        put(0x80 + i, f"MOVX A,{r}", 1, 2)
        put(0x90 + i, f"MOVX {r},A", 1, 2)
    for i, p in enumerate(("P4", "P5", "P6", "P7")):
        put(0x0C + i, f"MOVD A,{p}", 1, 2)
        put(0x3C + i, f"MOVD {p},A", 1, 2)
        put(0x9C + i, f"ANLD {p},A", 1, 2)
        put(0x8C + i, f"ORLD {p},A", 1, 2)
    for i, p in ((1, "P1"), (2, "P2")):
        put(0x08 + i, f"IN A,{p}", 1, 2)
        put(0x38 + i, f"OUTL {p},A", 1, 2)
        put(0x88 + i, f"ORL {p},#", 2, 2)
        put(0x98 + i, f"ANL {p},#", 2, 2)
    for op, mn, n, c in (
            (0x00, "NOP", 1, 1), (0x02, "OUTL BUS,A", 1, 2), (0x03, "ADD A,#", 2, 2),
            (0x05, "EN I", 1, 1), (0x07, "DEC A", 1, 1), (0x08, "INS A,BUS", 1, 2),
            (0x13, "ADDC A,#", 2, 2), (0x15, "DIS I", 1, 1), (0x16, "JTF", 2, 2),
            (0x17, "INC A", 1, 1), (0x23, "MOV A,#", 2, 2), (0x25, "EN TCNTI", 1, 1),
            (0x26, "JNT0", 2, 2), (0x27, "CLR A", 1, 1), (0x35, "DIS TCNTI", 1, 1),
            (0x36, "JT0", 2, 2), (0x37, "CPL A", 1, 1), (0x42, "MOV A,T", 1, 1),
            (0x43, "ORL A,#", 2, 2), (0x45, "STRT CNT", 1, 1), (0x46, "JNT1", 2, 2),
            (0x47, "SWAP A", 1, 1), (0x53, "ANL A,#", 2, 2), (0x55, "STRT T", 1, 1),
            (0x56, "JT1", 2, 2), (0x57, "DA A", 1, 1), (0x62, "MOV T,A", 1, 1),
            (0x65, "STOP TCNT", 1, 1), (0x67, "RRC A", 1, 1), (0x75, "ENT0 CLK", 1, 1),
            (0x76, "JF1", 2, 2), (0x77, "RR A", 1, 1), (0x83, "RET", 1, 2),
            (0x85, "CLR F0", 1, 1), (0x86, "JNI", 2, 2), (0x88, "ORL BUS,#", 2, 2),
            (0x93, "RETR", 1, 2), (0x95, "CPL F0", 1, 1), (0x96, "JNZ", 2, 2),
            (0x97, "CLR C", 1, 1), (0x98, "ANL BUS,#", 2, 2), (0xA3, "MOVP A,@A", 1, 2),
            (0xA5, "CLR F1", 1, 1), (0xA7, "CPL C", 1, 1), (0xB3, "JMPP @A", 1, 2),
            (0xB5, "CPL F1", 1, 1), (0xB6, "JF0", 2, 2), (0xC5, "SEL RB0", 1, 1),
            (0xC6, "JZ", 2, 2), (0xC7, "MOV A,PSW", 1, 1), (0xD3, "XRL A,#", 2, 2),
            (0xD5, "SEL RB1", 1, 1), (0xD7, "MOV PSW,A", 1, 1), (0xE3, "MOVP3 A,@A", 1, 2),
            (0xE5, "SEL MB0", 1, 1), (0xE6, "JNC", 2, 2), (0xE7, "RL A", 1, 1),
            (0xF5, "SEL MB1", 1, 1), (0xF6, "JC", 2, 2), (0xF7, "RLC A", 1, 1)):
        put(op, mn, n, c)
    return t


def norm(text):
    """A mnemonic as the table spells it: no spaces, 0 for O in R0/P0."""
    text = text.upper().replace(" ", "").replace("\t", "")
    return text


def check_listing(path):
    """Each listing line 'AAA:OP ...  MNEMONIC operands' against the table."""
    t = table()
    bad = seen = 0
    for line in open(path, encoding="utf-8"):
        # 'AAA:OP [XX]<tab>[label]<tab>MNEMONIC<tab>operands ; comment'
        m = re.match(r"^([0-9A-F]{3}):([0-9A-F]{2})(?: [0-9A-F]{2})?\t", line)
        if not m or "│" in line:
            continue
        fields = [f.strip() for f in line.split(";")[0].rstrip("\r\n").split("\t")[1:] if f.strip()]
        if len(fields) > 1 and re.match(r"^[Ll]_[0-9A-Fa-f]+$", fields[0]):
            fields = fields[1:]
        op = int(m.group(2), 16)
        mnemonic = norm(" ".join(fields[:2]))
        want, _, _ = t.get(op, ("?", 0, 0))
        stem = norm(want)
        # the table's operand placeholders: '#' (immediate), ',' (a jump's target), none (an address)
        if not (mnemonic == stem or (stem.endswith(("#", ",")) and mnemonic.startswith(stem))
                or (stem in ("JMP", "CALL") or stem.startswith("J")) and mnemonic.startswith(stem)):
            bad += 1
            print(f"{m.group(1)}: {op:02X} listing {mnemonic!r}, table {want!r}")
        seen += 1
    print(f"{seen} instructions of the listing, {bad} against the table")
    return bad


if __name__ == "__main__":
    t = table()
    if len(sys.argv) > 1:
        sys.exit(1 if check_listing(sys.argv[1]) else 0)
    print(f"{len(t)} opcodes")
