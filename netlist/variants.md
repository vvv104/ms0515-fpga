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

- **SA1** (switch k joins pins k and 21-k) routes the socket XS2: the odd
  set (1, 3, 5, 7, 9) carries +12V, the keyboard's line out (`ВЫХКЛ L`),
  its line in (`ВХКЛ L`), the cassette read through E8... see
  `nets.csv` sheet 1; the even set the cassette and the С2 line - so the
  odd builds are the МС 7004 keyboard machines, the even ones the МС 7007.
- **E3 / E4** tie `РВК1 Н` or `РВК0 Н` to ground: they pick the sync PROMs'
  page, i.e. the video timing variant.
- The sheet's note: «Перемычки E1, E2, E3, E4, E5, E7, E8 выполнены
  проводом НВМ-0,12.4.500 ГОСТ 17515-72» - E6 is not in the list.

`nets.csv` holds every jumper and every SA1 contact as drawn; a build is
chosen by which of them are closed.
