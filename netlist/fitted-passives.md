# The passive parts on the owner's board, as far as the photographs show

Read 2026-11-08 off `MC 0515 04.jpg` at x5 and x6 (crops in the owner's
`photos/ask/`, 9 to 13).  ПЭ3's counts for the board: 80 resistors (17 x
1 kOhm, 7 x 20, 5 x 75, 5 x 150, 4 each of 560, 510, 470, 330, 220, 24 Ohm,
3 x 10 kOhm, 3 x 390 ...), 87 capacitors (65 x К10-17-2б Н90 0.22 uF, 4 x
К53-14 33 uF 16 V, 4 x 2200 pF, 3 x 100 pF, 3 x 51 pF, one each of 4700,
120, 68, 27 pF, 3 x Н90 of another size), 13 КД522Б diodes, 3 АЛ307БМ
LEDs, two resistor networks.  The photograph tells the kind and the place
of each part; the colour bands of a 0.26 mm/px photograph are not a
reading, and are given as seen, not as values.

## What is certain

- **The two resistor networks** НР1-11-3-15: DA1 1 kOhm ±5 % (8908), DA2
  10 kOhm ±5 % (9011) - read off the packages (`fitted.csv`).
- **C70 is not fitted**: two pads with the leads' stubs in the solder
  (docs/findings.md, "C70 on the boards seen").
- **Three red LEDs** at the board's lower right corner (HL1-HL3, АЛ307БМ)
  with their three series resistors above them: yellow-violet-brown-gold
  as seen = 470 Ohm, which is ПЭ3's 4 x 470 (one more elsewhere).
- **Four axial electrolytics** at the upper left corner by the supply
  connector, two and two, «+» marked on the board: К53-14 33 uF 16 V by
  ПЭ3 (the photograph shows grey cans with white ends; the type's
  printing is turned away).
- **The yellow ceramic capacitors** К10-17 everywhere between the chips:
  the decoupling 0.22 uF (65 of them); the one read says «2F8 .22».
- **Orange drop capacitors** (К10-17-1б, the picofarad values) along the
  upper edge by the connector and among the top rows.
- **The red glass diodes** КД522Б: two at the left end of the resistor row
  by the «ОМЕГА-3» sticker (the С2 port's), one at the left end of the
  seven-resistor row under the module's edge, a vertical row of seven to
  the right of the processor (the keyboard's and the С2's lines), two
  more by the top strip - 12 placed of the 13.
- **The 8 MHz crystal** BQ1 by D15 («8000 / ЕГН / 4184», the owner's
  reading); **BQ2** by D91, its marking turned away (`fitted.csv`).

## The resistor groups (`photos/ask/`)

| crop | where | count | bands as seen | ПЭ3's neighbourhood |
|---|---|---|---|---|
| `9-resistors-top-strip.png` | right of D20/D28, above D48 | 7 (one red diode at the left) | brown-black-red; brown-black-orange; yellow-violet-red; ... (unsure) | the С2 port's pull-ups and the receiver's divider |
| `10-resistors-omega-row.png` | under the capacitor cans, left of the «ОМЕГА-3» chip | 2 diodes + 9 green/yellow + 1 slanted | the green-bodied ones carry two or three bands, unread | the С2 port's lines (D20/D28/D48) |
| `11-resistors-above-DA2.png` | above DA2 and D93, right of the ВВ55s | 11 | brown-black-red (1 kOhm) on several; one yellow-violet-brown | the joystick/ВВ55 port's pull-ups: ПЭ3's 17 x 1 kOhm live here and by the keyboard |
| `12-resistor-pairs-by-D14.png` | two pairs beside D14 (ЛА3) and D30/D36 (ИЕ10) | 2 + 2 | red-red-brown or orange-orange-brown (220 or 330 Ohm) | the video's R7/R8 pull-ups (1 kOhm) are *not* these - they stand by the sync PROMs' jumpers |
| near the LEDs | lower right | 3 | yellow-violet-brown (470) | HL1-HL3's series resistors |

The owner may read the bands off the crops where the camera did not; a
value that matters to the model is only R39 (1 kOhm, the `БОРДС L`
edge with C70) and R7/R8 (the page jumpers' pull-ups), and none of the
groups above is them.
