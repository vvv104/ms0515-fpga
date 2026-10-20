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

From the photo `MC 0515 04.jpg` (jumpers read by the owner 2026-10-20,
placed by the photo fit and the pads' copper):

- **E4 fitted** (a white wire from the `РВК0 Н` pad to a plane pad),
  **E3 open** - in a row of four positions whose other two are R7, R8;
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
