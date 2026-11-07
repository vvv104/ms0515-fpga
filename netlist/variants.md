# Board variants

Э3 sheet 1 (page 18) gives four builds of `3.858.420`; they differ in the
ROM pair, the switch SA1 and the jumpers E3/E4.

| Build | D65 | D77 | SA1 contacts closed | E3 | E4 |
|---|---|---|---|---|---|
| 3.858.420 | 7004L | 7004H | 1, 3, 5, 7, 9 | not fitted | fitted |
| -01 | 7004L | 7004H | 1, 3, 5, 7, 9 | fitted | not fitted |
| -02 | 7007L | 7007H | 2, 4, 6, 8, 10 | not fitted | fitted |
| -03 | 7007L | 7007H | 2, 4, 6, 8, 10 | fitted | not fitted |

The ROMs are К573РФ4Б programmed per `0.348.422-04ТУ`.

- **SA1** (switch k joins pins k and 21-k) decides what the socket XS2
  carries:

  | XS2 | odd set closed (1, 3, 5, 7, 9) | even set closed (2, 4, 6, 8, 10) |
  |---|---|---|
  | 1 | +12V | `ЗАП МАГ Н` (cassette write) |
  | 2 | `ВЫХКЛ L` (line to the keyboard) | GND |
  | 3 | `ВХКЛ L` (line from the keyboard) | `ЧТМАГ Н` through E7 |
  | 4 | GND | `ВЫХКЛ L` |
  | 5 | `СТЫК С2 108` | `ЧТМАГ Н` through E8 |

  So the odd builds put the МС 7004 keyboard (power and its serial line)
  on XS2; the even builds, fitted with the 7007 ROMs, put the cassette
  recorder there.
- **E3 / E4** tie `РВК1 Н` or `РВК0 Н` to ground: they pick the sync PROMs'
  page, i.e. the video timing variant.
- The sheet's note: «Перемычки E1, E2, E3, E4, E5, E7, E8 выполнены
  проводом НВМ-0,12.4.500 ГОСТ 17515-72» - E6 is not in the list.

`nets.csv` holds every jumper and every SA1 contact as drawn; a build is
chosen by which of them are closed.

## The owner's board

From the photo `MC 0515 04.jpg` (2026-10-20, placed by the photo fit
and the pads' copper) - but see "The frame rate" below: at the photo's
2000 x 1692 the pads of E3 and E4 cannot be told apart, and what follows
for them is the assembly drawing's list, not a reading:

- **E4 fitted** (a white wire from the `РВК0 Н` pad to a plane pad),
  **E3 open** - in a row of four positions whose other two are R7, R8 -
  by the drawing; the owner looked at the photo again on 2026-11-07 and
  sees nothing there to read;
- **E1 fitted** (a white wire in place of a resistor, between D15.8 and
  the D16.1 pad), as the sheet-1 note on wire jumpers says;
- **E5 fitted** (a white wire left of D91, from the pad on `ТС15 Н` -
  copper to D37.2 - to the pad on D91.6): the 15 MHz oscillator into the
  clock net;
- **E2 fitted** (the white wire beside VD1 and R6, by D20/D28): С2's
  circuit 108 from D20.6 to XP5.5;
- **E7 and E8 fitted** (the pair of white wires by the two-pin XP7): the
  cassette input `ЧТМАГ Н` to SA1 contacts 6 and 10;
- E6 not seen (the speaker signal to XP6.11, for a monitor's speaker).

That is exactly the assembly drawing's list of jumpers to fit (note to
page 27: «E1, E2, E4, E5, E7, E8 - установочный размер 12,5»), E3 and E6
left out: the owner's board carries the drawing's standard set.

E4 fitted / E3 open is the base build `3.858.420` or `-02`; with the МС
7004 keyboard the owner used, the base build: ROMs «7004L»/«7004H» and
SA1 1, 3, 5, 7, 9 by the table.  The ROMs in it in 2015 were ROM-B and
unlabelled (`docs/findings.md`) - one more sign they were replaced.

## The frame rate the jumpers give

E3 and E4 pick the sync PROMs' page (D13 A9 = `РВК0 Н`, A8 = `РВК1 Н`; D21
A7 = `РВК0 Н`), and software reads the same two lines at 177602 bits 3, 4.
The ТО's Table 10 and the PROM dumps (`firmware/`) agree:

| РВК1 | РВК0 | Line (D21) | Lines a frame (D13) | Frame |
|---|---|---|---|---|
| 0 | 0 | 64 us | 312 | 50 Hz |
| 0 | 1 | 53.3 us | 262 | 72 Hz |
| 1 | x | 64 us | 260 | 60 Hz |

With E4 fitted and E3 open, `РВК0` is low and `РВК1` high through R8:
60 Hz, which the generated board runs at (`sim/board/test_board.cpp`).
50 Hz needs both jumpers fitted, which no build of the sheet-1 table
has - and yet that is what the machines in use had (2026-11-07, the
owner's argument, borne out by the circuit):

- the timer interrupt, vector 100, is the frame sync and nothing else:
  D60.1 (К555ТМ2) has the enable bit `РПРРТ H` on D and `КСИ H` on its
  clock, the ВИ53 is not in the path - so an RT-11 monitor's clock
  ticks once a frame;
- the RT-11 kits of two bureaus, Omega's and OSA's, are generated for a
  50-cycle clock (`CLOCK = 50.`) and read nothing off `РВК`; at 60 Hz
  their TIME would gain 4.8 hours a day;
- the machines kept time in banks - the owner's own came from one, a
  diskette of his says so, and its board carries an «ОМЕГА-3» sticker.

So the bureaus fitted E3 beside E4 for the 50 Hz monitors they shipped
with, and the owner's board ran at **50 Hz**, 312 lines.  The emulator
starts at 50 Hz (its `dip_refresh` 0; for a day in 2026-11 it started at
60 on the strength of the drawing's list).  The generated board still
has the drawing's E4 alone; giving the owner's variant E3 too (312
lines, 20 ms, the flash at 3.1 Hz) changes every frame count the
baseline measured and is a step of its own, with every corner's tests.
