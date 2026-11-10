# НС4 anew: for the fab and the bench

The board of `docs/plan.md` 8: Э3's system board (НС4 rev. 3А) on eight layers,
every part where the 1991 board had it, the outer layers Mick's traces of the
1991 copper (zx-pk.ru 15146 #271), the inner ones routed anew
(`tools/pathfind.py`).  It is `kicad/ns4.kicad_pcb`.  Checked: every pad on its
netlist net (`tools/pcb.py check`), the copper read back without its net names
equal to the netlist (`tools/readback.py`), KiCad's DRC no error and nothing
unconnected, the supplies (`docs/pcb/power.md`).

Not built yet: nothing here has been on a bench.

## The files

`python tools/fab.py` (KiCad's command line as `$KICAD_CLI` or on the PATH)
writes `build/fab/ns4/`:

- `ns4-gerbers.zip` - what a fab takes: the eight copper layers, both masks,
  both silkscreens, the outline, the drill files (plated and not, apart) and
  their maps, and KiCad's job file;
- `assembly-top.pdf` - the parts' outlines and names on the parts' side;
- the parts list is `docs/pcb/bom.md` (`tools/bom.py`).

## What to order

| | |
|---|---|
| Layers | 8, in this order: F.Cu (1), In1 (2), In2 (3, the GND plane), In3 (4), In4 (5), In5 (6, the +5 V plane), In6 (7), B.Cu (8) |
| Size | 290.1 x 230.1 mm, 1.6 mm FR-4 |
| Copper | 35 um outside; inside 35 um is what `power.md` reckons with (18 um doubles its static drop, still under 10 mV) |
| Narrowest track, gap | 0.19 mm, 0.2 mm (the 1991 runs 0.25 mm, narrowed only where the traces' error leaves less room) |
| Holes, plated | 0.5 mm x 819 (vias, every one on a hole of the 1991 board), 0.8 x 2150, 0.9 x 6, 1.0 x 268, 1.2 x 9, 3.2 x 3 |
| Holes, bare | 2.85 x 2, 3.2 x 4, 4.8 x 6 (the 1991 board's mounting holes) |
| Finish | any (HASL, ENIG); mask and silkscreen both sides |

A board this size on eight layers is not cheap; the price is the size, not a
fine rule - nothing here needs more than a fab's standard eight-layer process.

## Before ordering: what to hold against the real parts

The holes stand where the 1991 board's are; their diameters and the parts'
bodies are partly read, partly reckoned.  Worth checking with the parts in hand:

- ОНП-ЖГ-79 (XP3, XP5, XP6): the pins' pitch (~2.9 mm, from the 1991 holes)
  and their diameter (1.0 mm);
- ОНЦ-КГ-4-5/16-Р (XS2, DIN-5): drawn by hand (`kicad/ns4.pretty`);
- СП5-16ВА (R27, R31, R40, R50): the corner of three pins;
- К53-14 (C1, C2, C10, C11) 25.4 mm, К10-17 5.08 mm, the resistors 12.7 mm.

## The options

As the netlist has them (`netlist/analog.csv`; why: `netlist/variants.md`):

| Part | As delivered | What it does |
|---|---|---|
| E1 | fitted | a wire in place of a resistor |
| E2 | fitted | С2 circuit 108 to XP5.5 |
| E3, E4 | fitted | РВК1 Н, РВК0 Н to ground: with both, the 50 Hz page (the machines in use ran at 50 Hz) |
| E5 | fitted | the 15 MHz oscillator into ТС15 Н |
| E6 | not fitted | the speaker to XP6.11 |
| E7, E8 | fitted | ЧТМАГ Н to SA1's contacts 6 and 10 |
| E9 | 1-2 | the board's own: D80.5 on `ВБРЕГ L` as Э3 draws it; 2-3 for the RAM-disk module (`netlist/pcb_parts.csv`) |
| SA1 | 1-20, 3-18, 5-16, 7-14, 9-12 closed | the base build: the МС 7004 keyboard on XS2 |
| XS3, XS4 | ROM-B | the two К573РФ4 (D65, D77) in sockets; ROM-A runs Rodionov's disk, ROM-B the Omega kit (`docs/findings.md`) |

## The first power-on

A laboratory supply with current limits; the techdesc's figures (table 2) are
the most the board draws: +5 V 2 A, +12 V 0.1 A, -12 V 0.02 A.

1. The bare board: no short between +5 V, +12 V, -12 V and GND (an ohmmeter,
   both ways).
2. The passives, the sockets and the connectors fitted, no chip: the same again.
3. The chips (the four socketed ones left out): power with the limits at 1 A,
   0.1 A, 0.05 A; the currents well under the techdesc's.  The two
   oscillators running: 8 MHz on BQ1 (D15), 15 MHz on BQ2 (D91).
4. ВГ93 (XS5), the processor (XS6), the ROMs (XS3, XS4) in their sockets; the
   limit on +5 V at 2.5 A; a monitor on XP6 and the keyboard - the ROM's own
   start, as on the machine.

## Known risks

- **640 x 200's left edge** (`docs/findings.md`, plan 6b/6d): on the model Э3's
  board draws it right only with the chain to D70 at the serial machines' speed;
  on copper the chips as fitted decide it.
- The inner layers are new, not the 1991 board's (no scan shows them); what
  they must join, to ring out on a real board: `docs/pcb/inner-layers.md`.
