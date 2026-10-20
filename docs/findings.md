# Findings

Places where the sources disagree with each other, with the board, or with
the software.  Each entry: what each source says, what was decided, and who
decided it.

## Interrupt priority (Э3 sheet 2 vs. OSA)

- **Schematic** (page 19, D96 К555ИВ3): КЛ > ТАЙМ > М С2 > Д С2 > 7007 >
  МОН.
- **Software**: with МОН last, OSA's ^Q hangs about half the time in the
  emulator (its 064 service re-requests before RTI).  The emulator puts
  МОН first.
- **Open**: which revision the Э3 shows; the board scans (rev. 3А) are to be
  traced around D89/D96 to see whether the copper agrees with the sheet.

## XP5 pin 13 (Э3 sheet 2 vs. ПЭ3)

- **Э3 sheet 2**: D97 Q4 (pin 12) `НАЧ Н` goes out as `ХР5:13` (read
  so by the user too).
- **ПЭ3**: XP5 is `ОНП-ЖГ-79-9` - nine contacts.
- **Open**: the board scans decide where pin 12 of D97 really goes; until
  then the connection is kept out of `netlist/nets.csv`.

## D23 type (ПЭ3 vs. the power table of Э3 sheet 1)

- **ПЭ3** (page 24): D23 КР531ЛЕ1.  **Power table**: К555ЛЕ1.
- Same function and pinout (quad 2-NOR); the series differ in speed.
  `parts.csv` keeps ПЭ3's; the chip on a real board decides.

## КР1807ВМ1 vs. T-11

- The pin functions drawn on Э3 sheet 2 match the T-11 User's Guide pin
  diagram for all 40 pins.  Whether the microcycle timing and the details
  of behaviour are the same is not documented in the sources at hand: the
  Soviet specification (ТУ 0.348.964-01) is wanted.  Every place the
  processor model leans on the T-11 documents is to be marked as such.

## СГИ L and КГИ L are one wire (Э3 sheet 4)

- D21 O1 is labelled `СГИ L` (line sync pulse), D13 O1 `КГИ L` (frame
  sync pulse).  At D14.3 both labels end on one vertical wire with a dot
  on each, so the two open-collector outputs are tied: a composite sync,
  pulled up by R5 alone (`КГИ L` has no pull-up of its own).  The netlist
  keeps them as one net, `СГИ L`.  To confirm on the board.

## Transistor arrays DT1/DT2 (Э3 sheet 4) = VT1/VT2 (ПЭ3)

- Э3 sheet 4 draws the video output stage with `DT1` (КТ677) and `DT2`
  (КТ676); ПЭ3 lists `VT1 КТ677АС` and `VT2 КТ676АС` and no DT parts.
  The netlist uses ПЭ3's references.

## Wire patches on НС4 (forum)

- Radon17, zx-pk.ru 15146 post #251 (2023-09-08): «на НС4 есть патчи
  проводками (они даже на монтажке отражены)» - the board carries wire
  patches, and they are drawn on the assembly drawing.  The copper alone is
  therefore not the machine: the patches on the assembly drawing
  (3.858.420 СБ, page 27) and on photos of real boards are to be read and
  added to the netlist as wires.

### The owner's board, 2015 (photo 16337efabef4, 1280x960)

A close-up of the owner's НС4 rev. 3А as handed over in 2015
(`$MS0515_DATA/photos/known-machines/16337efabef4.jpg`, from the Yandex
folder https://yadi.sk/d/5J5jsBP0fn-FSQ; the full-size file is lost):
the processor КР1807ВМ1 (date 9010), both ROMs К573РФ4А (8911, 9004; "Vpp=23В"),
the two ВВ55, ИР22/ИР23/АП5 - and several wire patches: a white wire
running down beside the ROM sockets from near the processor, another from
the ЛИ6/ЛН2 group to the left, a green wire with a series part near the top
ИР22.  Two chips left of the ВВ55 carry hand-written labels «ОМЕГА-2» and
«ОМЕГА-3».  Their pins are to be found by laying the photo onto the
placement (pin 1 upper right); not done yet.

### Which ROM the owner's machine carried (checked 2026-10-20)

- **2012, KokaF77** dumped his machine's ROMs, labelled «7004L» (D65) and
  «7004H» (D77) (zx-pk 15146 p14 #134).  Merged by Titus, they are the
  ROM in EmuStudio v0.12g: md5 b92cda2d… - **ROM-A** (the one with the
  cassette loader).  The leningrad.su board's ROMs carry the same labels.
- **30.06.2015** the club-museum dumped the owner's ROMs at the hand-over
  (thread 9786 #814829); vvv104 posted them on 03.07.2015 (15146 #815382),
  anasana merged them (#815389, attachment 52762: "the content differs",
  "no reaction to the L command").  That file is md5 3d60d719… - **ROM-B**
  byte for byte (`$MS0515_DATA/roms-provenance/`).
- The owner's ROM chips (К573РФ4А, 8911 and 9004) carry no 7004 labels,
  only hand marks; ROM-B is the one the Omega system disks need
  (emulator KNOWN_ISSUES: the vvv104 Omega disk runs on ROM-B only;
  shattered 2017: Omega's lang.dsk wants "другая прошивка").
- Neither ROM drives the RAM disk (no access to its data port 177550 in
  either listing): the RAM disk is the OS's driver's business.
- The owner's account: the machine was bought, later taken back to the
  plant whole for the RAM-disk upgrade and came back with a different
  system diskette, the old one possibly no longer booting.  Everything
  above fits ROM-A at purchase and ROM-B (with Omega) after the upgrade;
  what is proven is only ROM-B in 2015.  The plant's people could have
  changed anything then - the green wire patch included.
- The owner also had Rodionov's disk (RT15SJ «©1992» + ROSA3, the
  Norton-like shell), which runs on ROM-A only and has no driver for the
  512 KB module (no EX.SYS; the Omega kit has it).  He recalls it as the
  disk that came with the upgrade, but is not sure.  The evidence orders
  it the other way (Rodionov/ROM-A before, Omega/ROM-B with the RAM disk
  after); left open.  For the FPGA it means: both ROM contents are needed,
  switchable.

### The green wire on the owner's board (photo `MC 0515 04.jpg`)

The photo laid onto the placement by six chip centres (worst error ~24
layer px, under a pin pitch; the pin labels then sit on the pins):

- **one end: D92 pin 9** - Q3 of the address latch, `АД03 Н`;
- **the other end:** the upper lead of a discrete resistor between D80
  (К555ИД7, the register decoder) and D85, next to D80 pins 4/5 (G2A/G2B,
  both `ВБРЕГ L` on Э3).  Its reference on the assembly drawing is
  unreadable; in Mick's traces its pad is on the run the traces wrongly
  merge (`АД13`/`Р3`), its other lead on a plane.  Not settled.

Why it is there - a hypothesis the documents support:

- Alex_K (zx-pk 15146 #315, 2026) read the decoder off Э3: D80 decodes
  `АД05`-`АД07`, "the line `АД03` I do not see used";
- the memory expansion module's connection tables (`docs/ext`) decode
  its own registers with `АД03` (into its D10) and take "выбор устр"
  from XS1:C23, ТС15 from C21, ДЧТ51 from C22, СТАРТ from C24 and the
  RS-232 lines from C17-C19 - **none of XS1 C17..C24 is wired on Э3's
  НС4**;
- Alex_K (#313): the RAM-disk board's registers overlap НС4's own -
  "либо были какие-то модификации, которые неизвестны".

So fitting the module needs НС4 to give up half of its register space
(gate its decoder with `АД03`) and to bring signals out to XS1 C17..C24:
the green wire from `АД03` towards D80's enables fits the first, and it
appears on the owner's board only - the one that got the module at the
plant.  To settle: a sharper look at the lower end (which pin, and
whether a D80.4/D80.5 trace is cut), and the XS1 C17..C24 contacts on a
board with the module.

### The factory wire patches restore Э3 (assembly drawing, 2026-10-20)

The assembly drawing 3.858.420 СБ (page 27, drawn turned half round: a
"9" there reads "6" in a raw crop) draws the factory patches as thin
lines between pins, each end marked with the pin number.  Every one
traced so far joins pins that Э3 puts on one net and the copper does not:

| Patch (as drawn) | Э3 net | Copper without the patch |
|---|---|---|
| D13.1 - D84.4 | `АПК7 Н` | D13.1 with D36.11; D84.4 alone |
| D17.3 / D33.3 - D47.5 | `ИМП ЧТ Н` | D17.3 with D33.3; D47.5 alone |
| D15.9 - D40.9 | `ТС8М L` | D40.9 with D16.1 by my copper read - doubtful, see below |
| D98.3 - D70.11 | `СТРОБ Н` | D98.3 with D30.15/D37.7/D42.7 by my copper read - doubtful, see below |

The drawing's own notes (page 27, the text block beside the title, read
2026-10-20) say how: note 11 «Поднять ножки микросхем D84/4, D17/3,
D33/3» - those three pins are lifted off their pads and go to the wires
only.  No track cuts are noted anywhere.  So:

- D84.4, D17.3, D33.3: lifted and rewired - exactly the copper/Э3
  differences there;
- D40.9 and D98.3 get a wire **without** a lift or a cut.  If the copper
  joined D40.9 to D16.1 (the E1 side) and D98.3 to the ~D30.15 run, as
  my reading of Mick's traces says, the wires would short two outputs.
  So those two copper reads are the doubtful ones (like the D77.27 case,
  a run passing a ring), not the board: these places are closed in Э3's
  favour.
- The wire D98.3 is drawn as a long polyline along the bottom and up the
  right side; its far end is taken to be D70.11 (`СТРОБ Н`, the only
  other pin-numbered end on that route).  The stepped polyline in the
  column D17..D65 without pin numbers is the D13.1 - D84.4 wire's route:
  on the owner's photo (annotated copy `photos/known-machines/annotated-MC
  0515 04-wires.jpg`) one white wire leaves D13's pin row between the two
  РТ12s, runs down past D14 and D22, along the row above D17..D58 and turns
  down past D58 towards D84; the D98.3 - D70.11 wire comes in from the
  right along the row under D64/D71 and rises behind the brown capacitor
  (D64) to D70.  Both as drawn.  The owner followed the first on his
  photo end to end: from D13's lower right pin, down, right, down along
  the ROMs' blue socket, to pin 4 of a К555ЛЛ1 (date 8906) - D84.4.

On the owner's board (photo `MC 0515 04.jpg`, read by the owner
2026-10-20): two white wires meet on pin 3 of a КР1533ТМ2 (date 9002) -
one runs to pin 3 of the next КР1533ТМ2 past one ЛР11 (D17, D25 = ЛР11,
D33), the other runs long and down with several bends: the patch
D17.3/D33.3 - D47.5 of the drawing, as built.

A thin bare wire on the owner's board, not on the assembly drawing (found
by the owner, 2026-10-20): it runs under the three resistors left of D70
(R24, R25, R26 - their upper ends are on ~D51.1, ~D57.1 and `ВК2 Н` in
the copper) from a pad below them that no drawn copper reaches (a plane
or an inner layer) to D70's pin row, where the white wire D98.3-D70.11
lands too - within one pin on the photo.  D70 (К555КП11) has its fourth
section (pins 12-14) unused and its inputs open on Э3; a bare wire
tying them to a plane is the usual fix, which would make it harmless.
Which pin and which plane is open - not in the netlist.

So **Э3 is the patched board**: the copper had errors, the patches fix
them, and Э3 was drawn after.  This settles three of the six
"cannot tell from the scans" places (D16.1/D40.9, ~D30.15/СТРОБ, and the
`ИМП ЧТ` and `АПК7` splits) - in Э3's favour.  `rev3a.csv` loses its
D40.9 entry: the copper there is the unpatched state, not the machine.
The same white wires are on the owner's board and the leningrad.su one.
Still to trace on the drawing: the other end of D98.3's and D84.4's
lines beyond the tiles read, and any patch not yet found.

### The owner's board against another one (first look)

`MC 0515 04.jpg` (the owner's НС4 rev. 3А, full view) against
`mc0515-6.jpg` (the leningrad.su museum machine, Sergei Frolov 2009,
980x735 - turned 90° to match), both in
`$MS0515_DATA/photos/known-machines`:

- the same placement part for part: the same board design;
- **the white wire patches run the same routes** on both - from the top
  left (by the РТ12s) down and along the row above the ROMs to the right
  of the processor and down; from the ROMs' left down to the lower rows;
  the short one by the ЛН2 at the top; the loop at the left middle.  So
  these are factory changes, as Radon17 said (they are drawn on the
  assembly drawing), not one owner's repairs;
- **one green wire on the owner's board only**, on the right: from beside
  the processor down to the bottom rows by the LEDs.  A later factory
  change, a repair, or the owner's own (he fitted a joystick socket and
  does not remember how it was wired) - to be found which pins it joins;
- the other board's ROMs carry «7004L»/«7004H» labels, the owner's none;
  the other board has blue capacitors, the owner's yellow; processors
  9109 and 9010.

The other photo is too small to follow pins; the routes are compared by
shape only.

## The copper against the schematic: first pass (Mick's traces)

`tools/compare.py` lays the 103 DIP parts on the pads of Mick's layer
traces (pin 1 upper right, confirmed by the square pin-1 pads of the 3А
scans) and checks every schematic net.  Nine places come out where the
traces join pins the schematic keeps apart, e.g. D9.19 (A0, `АД01`) with
D1.6 (A1, `АД02`), or D65.2 (A12, `АД13`) with D77.27 (PGM, `Р3`).
Looking at them on the 3А scans warped onto the traces
(`tools/register.py`, ~2 px), the one checked so far is the traces': the
layer-8 run from D65.2 passes beside the ring of D77.27 with a nub, while
Mick's line ends on it.  So a join in the traces is a question for the
scans, not a finding against the schematic; each of the nine is to be
looked at on the scans, and the inner-layer runs (2, 7) can be judged
there only partly - they show through the laminate under the rings.

Where each of the nine stands (2026-10-20):

| Join in the traces | Link that joins | On the scans | Verdict |
|---|---|---|---|
| D65.2 `АД13` - D77.27 `Р3` | layer 8, D65.2 up to D77.27 | the run passes beside the ring of D77.27, a nub touches it | open - looks like the traces' error |
| D70.5 `ТС15 L` - D70.10 `~D71.8` | layer 2, via - D70.5 | no gold seen between the via and D70.5 (it may run under the tin) | **my misread of Э3**: pin 5 is on the D71.8 wire with pin 10 - Mick's redraw and the copper agree; fixed |
| D9.19 `АД01` - D1.6 `АД02` | layer 2, D9.19 - D1.6 | gold leaves D1.6 upward and goes under a layer-1 run, where it may turn | open - a short A0/A1 cannot be right; the schematic stands |
| D16.1 `~E1.2` - D40.9 `ТС8М L` | layers 7, 2, 8 through a square pad | every part of the chain is on the scans | copper differs from Э3, the factory patch D15.9-D40.9 (with a cut) restores Э3 |
| D86.11 `А13` - D9.6 `МК02`; D1.19 `УСТ` - D38.9 `СИНХР2` - D76.2 `ВКЛДВ`; D30.15 - D98.3 `СТРОБ`; D78.6 - D74.6 `А13`; D62.10 `АД06` - D79.9 `БОРДС` | long chains through vias on layers 2 and 7 | the inner runs are largely there; which via is wrong cannot be told - the scans show layers 2 and 7 only through the laminate | open - for another copy of the board or Mick's P-CAD file |

Э3 sheet 5, D40 pin 13 (4D): a re-read took the short bar under it for a
join with the clock line of pin 9; it is a ground symbol, as the first
read and Mick's redraw (below) both have it.

## Э3 and the board: how disagreements are kept

Э3 was drawn by hand, the board may have changed after it, and the
documents we have come from unknown copies - neither source wins by rule.
`nets.csv` stays Э3 as read; a real board's differences live in
`netlist/<board>.csv` (`rev3a.csv` for НС4 rev. 3А), one moved pin per row
with its evidence, and go in only when the copper or a photo shows them
beyond doubt.  `python tools/netlist.py check rev3a` applies and checks
them.  The FPGA is built from Э3 with the rev. 3А changes, each one
switchable; where a change matters, software on the model decides.

In `rev3a.csv` so far: nothing - D40.9 was taken out again once the factory
patch D15.9-D40.9 was found (see below).

## Mick's redraw of Э3 in P-CAD 2002 (2012)

Mick redrew the four logic sheets of Э3 in P-CAD 2002 in 2012 (zx-pk.ru
thread 18337 post 482134 and page 5; Yandex Disk
https://disk.yandex.ru/d/-BDz0FNek24y_A, 2021): `mc-0515.sch` (binary
P-CAD 2002) and `mc-0515.pdf` (vector, 5 pages); the connector sheet was
never drawn.  anasana sent the same files on 2026-10-20 with Mick's P-CAD
libraries (`$MS0515_DATA/scans/anasana-2026-10-20`).  It is a second,
independent reading of the same Э3: where it and `nets.csv` differ, one of
the two misread the scan.

`tools/mick_pdf.py` reads the netlist out of the vector PDF (wires, pin
stubs, bodies, labels by line width and geometry; bus entries dropped),
`tools/compare_mick.py` compares it with `nets.csv`.  Mick's library
numbers some types' pins its own way (КР580ВИ53 D0 on pin 1, likewise
ВВ51, АП5, ЛИ6, ИВ3); Э3 and the datasheets agree with `nets.csv`, and the
comparison maps Mick's numbers back.  First pass: 1331 signal pins in
both, 1042 under the same net name, 20 disagreements.  Settled by Э3 and
the copper so far:

- mine wrong, fixed: D70.5 (on the D71.8 wire); `ИМЧТ Н` is `ИМП ЧТ Н`;
- Mick's slips, `nets.csv` kept: D62/D67 swapped in the address-mux
  stack (copper: D62.5 with D74.6 `А13`, D62.11 with D74.8 `А14`); D84.12
  `АД13`/D84.13 `ДПЗУ` swapped (copper: D84.12 on the `АД13` run); D7.14
  `МК08`, not `МК06` (copper: with D3.2, D58.18, D77.11); D81.9 on the
  D69.9 wire, as the user read it (copper: D81.1, D81.4, D81.9 one run);
  `ТС45 L` on D70.3/D70.6 is `ТС15 L` (the label points to sheet 3, where
  only `ТС15 L` is made);
- `СГИ L`/`КГИ L` on D14.9/D14.10: Э3 joins them with a dotted wire and
  the copper joins the two pins; Mick keeps them apart.
