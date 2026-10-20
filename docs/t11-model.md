# The КР1807ВМ1 model: decisions

`docs/t11.md` holds what the DEC documents say; this page holds what the
model makes of it where they leave a choice, and why.  The module is
`rtl/chips/kr1807vm1.sv`, its tests `sim/chips/test_kr1807vm1*.cpp`.

## The mode НС4 gives it

While `УСТ L` (-BCLR) is low, D95's first half drives МК08..МК11 from its
inputs (D95.1 1G is `УСТ L`): МК09 from ground through the inverter, so 1;
МК08, МК10, МК11 from `Р2 H` (pulled up by DA1), so 0.  Nothing else drives
the DALs then, and the pull-ups make the rest 1:

| MR bit | Level | Meaning |
|---|---|---|
| 15:13 | 111 | start 172000, restart 172004 |
| 12 | 1 | user mode |
| 11 | 0 | 16-bit bus |
| 10 | 0 | 64K dynamic (no effect: static) |
| 9 | 1 | static memory: AI are request inputs (the board wires them to D96, Р15, Р3, `ОСТАНОВ`) |
| 8 | 0 | normal R/W |
| 1 | 1 | standard microcycle |
| 0 | 1 | COUT once per microcycle |

The model reads the mode register as the chip does, but implements the
bus only for the modes it can be checked against: 16-bit, static, normal
or delayed R/W, standard microcycle, either COUT.  The other bits are
held and reported (a port of the test bench sees them); a board that
selects an unmodelled mode is out of scope.  The 8-bit bus, dynamic
multiplexing on AI, refresh counters on AI and the long microcycle are not
modelled.

The clock is `ПТС H`, D24's second flip-flop dividing `ТС15 L`: 7.5 MHz,
T = 133 ns, 16 periods of the 120 MHz master clock.

## The clock inside

XTL1 is sampled on the master clock; its rising edges are the phase
boundaries, its falling edges the half-phases.  A microcycle is φ2, φW, φ1
(3T), with φD between φ2 and φW in ASPI and IACK.  A transaction begins
at φ2, as every timing sheet draws it.

Edges fall on XTL edges where the AC table allows; where it needs an edge
between two of them, the edge is a fixed number of master ticks after the
XTL edge (`*_DELAY` parameters, in ticks of the 120 MHz clock), which
stands for the chip's own internal delay.

## Read and write (16-bit static)

Times in T from the start of the address microcycle's φ2:

| t | Read | Write |
|---|---|---|
| 0 | DAL ← address; SEL ← 01 for a fetch, 00 else; R/-WxB stay high | DAL ← address; R/-WHB, R/-WLB ← byte enables (normal mode) |
| 1 | -RAS ↓, COUT ↑ | the same |
| 2 | COUT ↓ | the same |
| 2.5 | -CAS ↓, DAL float | -CAS ↓, DAL ← data; delayed mode: R/-WxB ↓ |
| 3.5 | PI ↑, SEL0 ↓ | PI ↑ |
| 4 | COUT ↑ | the same |
| 5 | COUT ↓, -CAS ↑: **read data latched** | COUT ↓, -CAS ↑ (delayed: R/-WxB ↑) |
| 5 + PI_FALL_DELAY | PI ↓: AI requests latched | PI ↓ |
| 5.5 + RAS_RISE_DELAY | -RAS ↑ | -RAS ↑ |
| 6 | next address | R/-WxB ↑ (normal mode), next address |

PI cannot fall on an XTL edge: it has to come between tPIP (≥ 2T-47 after
it rose, so ≥ 5T+20 ns) and tNHP (≥ T-43 before R/W rises at 6, so
≤ 5T+43 ns).  Checked against the AC table (ES §4.1, `t11.md` §5.8) at
T = 133 ns, PI_FALL_DELAY = 3 ticks (25 ns), RAS_RISE_DELAY = 2 ticks
(16.7 ns):

- strobes: tRAS 4.5T+17 ≥ 4T+35; tCAS 2.5T ≥ 3T-90; tPIP 1.5T+25 ≥
  2T-47; tRSC 1.5T ≥ T+10; tCSP T ≥ T-28; tRSP 2.5T ≥ 2T+10; tCSR
  2.5T+17 ≥ 3T-40; tPSC 1.5T ≥ 2T-75; tPSR 2T+17 ≥ 2T+14; tRHC 0.5T+17 ≥
  50 ns; tRHP 0.5T-8 ≥ 10; tPHC 25 ≥ 10;
- precharge: -RAS 1.5T-17 ≥ 2T-120; -CAS 3.5T ≥ 3T-5; PI 4.5T-25 ≥ 4T-33;
- address: tDSR T ≥ T-48; tDHR 1.5T ≥ T-12; tDSC 2.5T ≥ 2T-22; tDSP
  3.5T ≥ 3T-20; tCDE T ≥ T-18; tRDE 0.5T-17 ≥ T-118;
- write data: tCWD 0 ≤ 80 ns; tRWD 1.5T ≤ 2T+4; tWDP T ≥ T-83; tWSC
  2.5T ≥ 3T-150; tWSP 2.5T+25 ≥ 3T-110; tWSR 3T+17 ≥ 3T-55; tWHC T ≥
  T-28; tWHP T-25 ≥ T-88; tWHR 0.5T-17 ≥ T-118;
- normal R/W: tNSR T ≥ T-78; tNSC 2.5T ≥ 2T-37; tNSP 3.5T ≥ 3T-45;
  tNHR 0.5T-17 ≥ T-108; tNHC T ≥ T-32; tNHP T-25 ≥ T-43; tNMP 6T ≥ 6T-66;
- COUT: tOPW T ≥ T-33; tORT 2T ≥ 2T-37; tMOR 0 ≥ 0; tMRO T ≥ T-51; tOSC
  0.5T ≥ 10;
- fetch SEL0: tFFR T ≥ T-23; tSSF 3.5T ≥ 3T-38.

`sim/chips/test_kr1807vm1_bus.cpp` measures every one of these on the
pins.

Read data are latched at the trailing edge of -CAS, as both documents
state (ES p.10; UG §2.3.2).  The table's read-data maxima (tDRD, tCRD,
tPRD) bind the memory, not the latch: a slower memory still reads right in
the model.

## Bus NOP and ASPI

A bus NOP is 3T with every strobe idle and DAL holding the last address.
ASPI (φ2 φD φW φ1, 4T): -CAS ↓ at 0, PI ↑ at 1, COUT in φW (2..3), -CAS ↑
at 3, PI ↓ at 3.5 with the requests latched.

## IACK

φ2 φD φW φ1 (4T): SEL1 ↑ and DAL<12:8> ← the acknowledged request at 0;
-RAS ↓ at 1; the vector (with -VEC) read from DAL<7:2>, latched at SEL1 ↓
at 3.5 (the ES gives the hold against SEL1); -RAS ↑ IACK_RAS_DELAY = 6
ticks (50 ns) after SEL1 ↓ (tRHS ≥ 45 ns).  On НС4 -VEC is `Р3 H`, held
high: every vector is the internal one, so no IACK reads DAL.

## READY

A falling edge of READY while -RAS is low asks for one slip; READY
already low when -RAS falls asks for one by itself (ES p.13), so READY
tied low slips every read, write, IACK and DMA grant once and leaves bus
NOPs and ASPIs alone.  A slip is one more microcycle (3T) before the data
microcycle ends: -CAS, PI and -RAS are held, COUT pulses once more, and the
read data and requests are taken a microcycle later (the ES READY sheet).
On НС4 READY is `ЗАДЕРЖ L` from D98.

## -BCLR

While -BCLR is low the DALs are let go whatever the transaction (tDFB),
so the board's mode drivers (D95) own them.

## DMA grant

Not used on НС4 (-DMR is `Р15 H`, held high), modelled for the pin's
sake: SEL 11 and DAL, R/-WHB, R/-WLB let go at 0; -RAS ↓ at 2; -CAS ↓ at
4.5 (tMRC); PI ↑ at 5.5; COUT in both φW; at 7 -CAS ↑ and SEL1 ↓, PI ↓
PI_FALL_DELAY later with -DMR sampled, -RAS ↑ SEL1_RAS_DELAY later; SEL0
↓ at 8 unless the grant goes on.  -DMR is read at every PI; the CP
requests only in reads and ASPIs.

## Choices between the documents

| Place | Chosen | Why |
|---|---|---|
| JMP/JSR with a register destination | vector 10 | ES App. A and B, and the emulator; UG and ES p.C-1 say 4.  To settle by software (plan 6b). |
| HALT saves PC, PS | on the stack | everywhere but one ES note |
| -PF level | not modelled further than its input | `Р3 H` holds it high on НС4 |
| RTT, RESET, WAIT times | App. B microcycles | the only source with microcycle counts; App. A's µs figures differ for these three |
| Vector strobe | SEL1 trailing edge | the ES's hold time is referenced to it |
| SP after power-up | 000376 | ES p.9, UG §3.5.2.5.  The emulator leaves 0. |

## Microcycle order inside instructions

The documents give totals (App. B), bus transactions (App. E) and eleven
example sequences (UG A-3).  The model's sequence for each instruction
matches all three: the fetch `R`, the decode `Ref` (a bus NOP here), then
the operand reads and writes in the order the instruction needs them, the
remaining non-bus microcycles placed where the example sequences put them
(address arithmetic before the access it serves, the rest at the end).
Where an example disagrees with App. B/E (`t11.md` §10.4), App. B/E win.
Each instruction's sequence is a row in the module and a test.
