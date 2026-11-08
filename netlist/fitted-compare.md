# Three boards side by side

`fitted.csv` (the owner's board, the plant's patch on D70.15 and C70 taken
off), `fitted-others.csv` (Frolov's and Radon17's boards, as Э3 draws them,
no patch).  Read 2026-11-08.

## Two production runs

The owner's board carries chips dated 8803 to 9010 (the ROMs 9004/8911,
the ВМ1 9106 and the ВИ53 9107 replaced later - `docs/findings.md`);
Frolov's and Radon17's carry 8912 to 9109 - the 1991 run, the boards
themselves dated 1991 (Radon17's 03.07.91; the owner's nameplate 07.1991
is the machine's, its board the earlier one).  Where the two runs put
different families:

| Position | Э3 (ПЭ3) | The owner's board (1990) | Frolov's / Radon17's (1991) |
|---|---|---|---|
| D16 ИЕ19 (the keyboard's counter) | К555 | К555 9009 | КР1533 9004 (Frolov, read at the limit) |
| D31 ЛА4 | К555 | **КР1533** 8912 | **К555** 8908 |
| D52, D58 АП5 (the bus buffers) | К555 | К555 8911 | **КР1533** 9101 |
| D61, D62, D67, D68, D87 КП2 (the colour multiplexers) | К555 | К555 9008 | **КР1533** 9105 |
| D63 / D69 КП2 | К555 | one К555 9008, one КР1533 9106 | both КР1533 (9105 Frolov, 9101 Radon17) |
| D64 ЛН2 | К555 | К555 0784 (an old brown ceramic) | **КР1533** 9104 |

Everything else is the same family on both runs: КР1533 at the five
ИЕ10, both ЛА3, ЛР11, both КП15, every ТМ2/ТМ8/ТМ9, the three ЛН1, ЛИ1,
ИД7, both ИР22, the five ИР23, ЛА2, АП3; К555 at КП11, the four ИР16,
ЛП5, ИЕ19 (D46, D59), the four ЛЛ1, ЛИ6, ЛИ3, ИВ3, the two ЛА13, ТЛ2;
К531 at ЛЕ1 and ТМ2 (D24); К155 at ЛП9 and ЛН5; РФ4А in the sockets.

## What it says for the 640 x 200 edge (6b)

The race's path (`docs/findings.md`, "What 640 turns on") runs through
D30 (ИЕ10), D21 (РТ12), R39/C70, D14 (ЛА3) and D70 (КП11), and the load
is taken by the four ИР16.  On all three boards those are the **same
families**: КР1533 at D30 and D14, К555 at D70 and the ИР16s.  So the
difference between the owner's board, which the plant patched here,
and the two that draw as Э3 is not in the chips' families on that path
- it is in the patch alone, or in the spread of delays within a family
(a slow К555КП11 or a slow РТ12 on one board).  Two things follow:

1. The model ran К555 delays at D30 and D14, where every board seen
   has КР1533 - faster by 5 to 10 ns a gate.  Now the generator's
   default (`fitted-1991.csv`); with the К555ИР16's MODE setup taken by
   corner, not at the sheet's guaranteed maximum everywhere, Э3's board
   draws the emulator's picture (`docs/findings.md`, "Settled,
   2026-11-08").
2. The owner's board, by his account a single machine the plant patched
   for him, carries its own repair; the serial boards needed none.

Not to be forgotten: the КП2 colour multiplexers (D61-D69, D87) are К555
on the owner's board and КР1533 on the 1991 run, and the model's item 3
(the one sample of ink before paper, through D63/D69's selection) is
timed on К555 delays - the 1991 boards would show less of it.
