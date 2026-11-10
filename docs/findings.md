# Findings

Places where the sources disagree with each other, with the board, or with
the software.  Each entry: what each source says, what was decided, and who
decided it.  The pictures behind an entry are in [evidence/](evidence/), a
folder per case.

## Interrupt priority (Э3 sheet 2 vs. OSA)

- **Schematic** (page 19, D96 К555ИВ3): КЛ > ТАЙМ > М С2 > Д С2 > 7007 >
  МОН.
- **Software**: with МОН last, OSA's ^Q hangs at every moment tried in the
  emulator (its 064 service re-requests before RTI).  The emulator puts
  МОН first.
- **The copper agrees with the sheet** (2026-11-01): on the rev. 3А scans
  all six runs from D89's outputs to D96's inputs are where Э3 draws
  them - МОН on I2, 7007 on I3, Д С2 on I4, М С2 on I5, ТАЙМ on I7, КЛ on
  I9 - as Mick's traces have them; the owner's photo shows no wire or
  lifted pin on D89 or D96.  So the owner's board, as far as the copper
  and the photo can tell, encodes in Э3's order, МОН last.  Only an
  inner-layer cut (the hidden layers are not on the scans) could still differ.
- **The board sticks as the emulator does** (2026-11-01, `run_board irq
  fetches=`): OSA (ROM-A, `TYPE LONG.TXT`) held by ^S pressed while it
  printed, then ^Q at 16 moments 7 ms apart: the output went on twice and
  stuck 14 times.  The mechanism, fetch for fetch, is the one the
  emulator shows:
  - held, the 064 service (147642) takes its request back (bit 8 of
    177400 cleared), finds the hold flag set (the word at 147704), opens
    the priority for one instruction (`MTPS #0` at 150170, `MTPS #340`
    at 150174), asks for itself again and returns; its RTI is followed at
    once by the next acknowledge, so the program it broke into never gets
    an instruction (one round 78 microseconds on the board, 64 in the
    emulator);
  - a key whose request comes before `MTPS #0` is read is taken in that
    window, with МОН's request taken back: the keyboard service (145744)
    lowers the priority itself (`MTPS #0` at 145776), goes on to 146002,
    sees ^Q, clears the flag - the output resumes (runs q4, q9: the
    request 2 microseconds before 150170);
  - a key whose request comes after it is taken at the RTI, above МОН's
    request asked again: at 145776 МОН nests in the keyboard service, its
    RTI returns to 146002 and the next acknowledge comes before 146002
    runs - for good; ^Q is never seen (run q0: the request 1 microsecond
    after 150174).

  So OSA's ^Q, with МОН last, depends on where in the 78-microsecond
  round the key comes, on the board built to Э3 as in the emulator built
  to the same order: the emulator was not hiding a second difference.
  Either OSA's machines did not encode МОН last (an inner-layer cut,
  another revision), or the T-11 takes the next request later than both
  models have it (after RTI, or after an `MTPS` that lowers the
  priority), or OSA's ^Q did hang there.  Nothing on the bench can tell
  which: a real machine with OSA can.
- Pictures: [evidence/irq-encoder](evidence/irq-encoder/).

## Brightness in 640 x 200 (Э3 sheet 4 vs. the emulator)

D69 (К555КП2) makes the brightness, D69.9, from ВК8 H and ВК5 H: its
second half takes `ИНТ Н`, the attribute's bit, on 2C0/2C1 and `ИНД3 Н (Л5)`
on 2C2/2C3 ([evidence/brightness-640](evidence/brightness-640/d69-sheet4.png)).
`ИНД3 Н` is D76's PC4, system register C bit 4 - the bit that also lights
the LED VD17.  So where 320 x 200 takes the brightness from each cell's
attribute, 640 x 200 takes it from register C bit 4: the board draws the
points dim with it clear.  The emulator draws 640 x 200 bright whatever
the bit (its Screen; `board.h` names bit 4 the LED only).

Found by the test ROM `firmware/tests/vid640.mac` (2026-11-03): with bit 4
clear every one of the 128 000 points differed from the emulator's picture
by the brightness alone (5 for 13); with it set, none.  The test keeps it
set.  The emulator is the one to change, if at all - a question for its
repository, and for software that draws 640 x 200 with bit 4 clear.

## R45: D86's OE - on GND, misread as on РПРРТ H (Э3 sheet 2)

- **What happened**: the netlist had D86's OE (pin 7) through R45 (20
  Ohm) on `РПРРТ H` = D97 Q0 = the dispatcher's bit 9.  On the board
  model Omega, which sets bit 9 (the timer on, ТО 4.3), lost `РАСШ H` -
  D86 off, the line floating to 1 - and wrote its first line into the
  VRAM's upper half: at row ~15, column 32 instead of row 2, column 0.
- **The sheet** (page 19): R45's wire ends in a ground sign, drawn right
  over the label «РПРРТ Н (Л2)» of D97.2's wire below it; the sign's bar
  merged with the letters in the reading (pointed out by the owner,
  2026-10-22).  So Э3 has R45 on GND, as R43 (D78), R62 (D92), R24, R25
  have their OEs: no drawing error - a misread, now corrected in
  `nets.csv`.
- **Everything else agrees**: with R45 on GND the model boots Omega from
  power-on to its prompt with the text where the emulator has it; the
  rev. 3А copper (Mick's layers 1, 2, 7, 8 and both scans) joins
  `РПРРТ H` to D97.2 and D60.2 only, through one via, and D86.7 and both
  ends of R45 have no outer-layer copper - pins on a plane; the assembly
  drawing (page 27) and the photos of the owner's and Frolov's boards
  have R41, R43, R45 stacked between D78 and D86, R45 next to D86.
- **The lesson**: where a wire ends on a label, look at the scan at full
  resolution for a sign under the letters before reading a net.
- **Pictures**: [evidence/r45-d86-oe](evidence/r45-d86-oe/).

## XP5 pin 13 (Э3 sheet 2 vs. ПЭ3): XP3 pin 13

- **Э3 sheet 2**: D97 Q4 (pin 12) `НАЧ Н` goes out as `ХР5:13` (read
  so by the user too).
- **ПЭ3**: XP5 is `ОНП-ЖГ-79-9` - nine contacts.
- **The copper** (Mick's traces, 2026-11-10): D97.12 runs on layers 2
  and 7 up to the board's edge, along it and down to **XP3 pin 13**, the
  last of the 25-pin connector's long row (XP3's other pins as Э3 has
  them: 1 `СТРБ H` - "ХР3:1" on the line above - 2..9 `ДКЛ00..07 L`, 11
  `ЗАВЕРШ H`, 12 `ОБСЛ H`).  So "ХР5:13" is ХР3:13, its 3 written as a 5.
- **Taken from the copper** (the owner, 2026-11-10): `НАЧ H` on XP3.13 in
  `nets.csv`.  The model's RTL does not change (nothing on the bench
  takes XP3.13); the printed boards have the pin on the net.  Pictures:
  [evidence/xp3-13](evidence/xp3-13/).

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

### The owner's board, 2015 (photo 16337efabef4, 2560x1920)

A close-up of the owner's НС4 rev. 3А as handed over in 2015
(`$MS0515_DATA/photos/known-machines/16337efabef4.jpg`, from the Yandex
folder https://yadi.sk/d/5J5jsBP0fn-FSQ; first a 1280 px preview, the full
size downloaded by the owner on 2026-11-03):
the processor КР1807ВМ1 (date 9010), both ROMs К573РФ4А (8911, 9004; "Vpp=23В"),
the two ВВ55, ИР22/ИР23/АП5 - and several wire patches: a white wire
running down beside the ROM sockets from near the processor, another from
the ЛИ6/ЛН2 group to the left, a green wire near the top ИР22 (read on the
preview as having a part in series; at full size it passes over one of the
board's resistors, nothing in series).  Two chips left of the ВВ55 carry hand-written labels «ОМЕГА-2» and
«ОМЕГА-3».  Their pins are to be found by laying the photo onto the
placement (pin 1 upper right); not done yet.

The machine itself (its case's plate, [evidence/owner-machine](evidence/owner-machine/)):
«ЭЛЕКТРОНИКА МС 0515.08», factory number 1X7XX48 (each X 0 or 9, the
punch struck only their tops), made 07.1991.

### Which ROM the owner's machine carried (checked 2026-10-20)

(Its RAM disk is dated in hand 09.11.93, the machine 07.1991 -
[evidence/owner-machine](evidence/owner-machine/): the upgrade came two years after the purchase.)

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
- **the other end, first read:** the upper lead of a discrete resistor between D80
  (К555ИД7, the register decoder) and D85, next to D80 pins 4/5 (G2A/G2B,
  both `ВБРЕГ L` on Э3).  The assembly drawing (page 27) names it **R39**:
  1 kOhm pulling `БОРДС L` up to +5V.  In Mick's traces its pad is on the
  run the traces wrongly merge (`АД13`/`Р3`), its other lead on a plane.
  Which lead the wire takes, and why, is not settled.
- **The owner's reading (2026-11-03): the wire ends on D80 itself, the
  fourth pin from the top of the row it comes down to** - roughly
  soldered, a lump of solder lying over R39 but touching neither its
  leads nor any other pin.  D80 lies turned a quarter on the photo: turned
  a quarter clockwise its label reads as text and that row is its lower
  one, pins 1..8 from the left - the fourth from the top is pin 5, **G2B**
  (the owner's reading of the turn, 2026-11-03).  On Э3 G2B is `ВБРЕГ L`
  with G2A.  `АД03` into one of the decoder's active-low enables: the board's
  registers answer only with `АД03` low - half its register space given
  up, as the hypothesis below wants.  For that the pin must be off
  `ВБРЕГ L` (lifted, or its run cut), or the latch and D88 would fight;
  the photo does not show which.
- **What it does.**  D80 (G1 `ПВАС Н`, G2A and G2B `ВБРЕГ L`, select
  `АД05`-`АД07`) picks one of the board's 040-byte register blocks and
  does not look at `АД03`, so every register of the board answers at its
  address and at that address + 010.  With G2B on `АД03` the board answers
  only with `АД03` low.  It loses nothing: every register it has is at an
  address with bit 3 clear (177440/442, 177460/462, 177500-506,
  177520-526, 177540-546, 177600-606, 177640-646, 177700/702,
  177720/722).  And the RAM disk's data port, 177550, has bit 3 set and
  falls in the block of the МС 7007's ВВ55 (177540-546), whose echo would
  answer there too and fight the module on the bus: the wire frees just
  the half of the register space the module uses.
- **Lifted or cut.**  The copper has all five pins of `ВБРЕГ L` (D88.8,
  the driver; D80.4, D80.5, D66.9, D98.12) on one visible run, its order
  not to be read off the traces.  Lifting pin 5 or cutting the run so
  that pin 5 alone is off it make the same circuit: D80.5 on `АД03`, the
  other four on `ВБРЕГ L`.  A cut that also took D80.4, D66 or D98 off
  D88 would hang them on `АД03` instead, and the machine, which worked,
  would not have.  Whether pin 5 still touches its pad the photos do not
  show (the owner, 2026-11-03), but it cannot: D88 (К555ЛА2) and D92
  (К555ИР22, its OE held low) are both push-pull, and would fight on
  `ВБРЕГ L` whenever `АД03` and it differ - most of the time; a TTL low
  winning, `ВБРЕГ L` would fall on every address with `АД03` low, memory
  ones too, and D80, D66 and D98 would act on them.  A machine that boots
  Omega with its RAM disk has no such fight.  And the joint itself points
  to a lifted pin (the owner, 2026-11-03): its lump of solder stands high,
  over R39, as a wire's does on the end of a pin bent up off the board -
  the way D70.15 is done - not down at the pad.  So: D80.5 off `ВБРЕГ L`
  (lifted, most likely), on `АД03 Н` - to
  go into `vvv104.csv` with the RAM disk (plan 6a), where it can be tested.
- Pictures: [evidence/green-wire-ad03](evidence/green-wire-ad03/).
- On the printed board made anew (2026-11-05, the owner) it is a jumper,
  E9, on both its versions: a mode of the machine, as Э3's E1..E8 are,
  not a repair - D80.5 on its middle contact, 1-2 `ВБРЕГ L` (Э3, as
  delivered), 2-3 `АД03 H` for the module.  `netlist/pcb_parts.csv`.

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

The board shows what the wire spares (2026-11-07, `docs/baseline.md`,
open list 1): booted from the owner's system diskette, which carries
EX.SYS, the module's handler, the board's keyboard is dead at the
prompt.  Omega's bootstrap runs the handler's installation check, and
that writes the module's ВВ55 at 177530-177536 - a control word, a
page of 1 to port A, 0 to port B.  With D80 as drawn those are D9's
registers: counter 0, the keyboard's baud clock, takes the 1 and stops
clocking D22.  With `АД03` into G2B the board's chips do not see the
module's half of the register space, and the keyboard lives.  The dec
kit's diskette has no EX.SYS and types.
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
  And the second: from D70's third pin from the top on the left (pin 11),
  under the short bare wire, down and right in steps to the КР1533ТМ2
  (9002) at the board's right edge, beside an unused connector - D98, the
  only КР1533ТМ2 on the board; it lands on the third pin from the bottom
  on the right - pin 3: the patch D98.3 - D70.11.

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
lands too - within one pin on the photo; the owner sees the white wire
pass under it, so the bare wire ends elsewhere on that row.  D70 (К555КП11) has its fourth
section (pins 12-14) unused and its inputs open on Э3; a bare wire
tying them to a plane is the usual fix, which would make it harmless.
Which pin and which plane is open - not in the netlist.

The owner read its ends (2026-10-20): from the first pin at the top on
the right of the chip on the left (the ИР23 D57 by the photo fit - its
corner pin there is **pin 10, GND**) to the second pin from the bottom on
the left of D70 - **pin 15, OE**.  On Э3, D70.15 is `~D14.3` (D14.3, the
NAND of `БОРДС L`/`БОРДК L`, also into D74.1): the strobe multiplexer is
switched off in the border.  The copper shows D70.15, D14.3 and D74.1 on
no outer-layer run (an inner layer), so a cut there cannot be seen.  A
bare wire from GND to D70.15 enables D70 always - the pixel strobes
`ВК2`..`ВК4` would run in the border too.  Candidates for the model
(plan 6b):
  a) as Э3 - D70.15 on `~D14.3`;
  b) D70.15 on GND, D14.3 still driving D74.1 (a cut hidden inside);
  c) D57's other corner is pin 11 (`ЧТ ВЗУ Н`) if the fit is off by the
     chip's orientation - then D70.15 on `ЧТ ВЗУ Н`.
b) changes only the border picture; the frame comparison will tell.

Settled the same day: the owner sees **D70 pin 15 lifted** off its pad.
So b) it is - D70.15 off `~D14.3` and on GND, D14.3 still driving D74.1;
in `vvv104.csv`.  Not on the assembly drawing and not on the leningrad.su
board as far as its photo shows: a later fix, most likely made by the
plant with the RAM-disk upgrade.  The owner looked at that board's photo
again (2026-11-03, `mc0515-6.jpg` as the museum serves it, 980 px - the
largest there is): no bare wire under R24-R26 to D70, and no green wire
from D92.9 either - the white factory wires are all there.  Both changes
are on the owner's board only, the one that had the module.  Pictures:
[evidence/d70-oe-gnd](evidence/d70-oe-gnd/); of the factory patches:
[evidence/factory-patches](evidence/factory-patches/).

What it does, on the model (2026-11-03..04, `run_board drawn` - Э3's
board - against the owner's, frames against the emulator's).  **Open:**
the model still makes Э3's board draw 640 x 200 wrong, the serial
machines did not (the owner: Omega and the other systems ran in 640,
the first characters of a line shifted would have been plain to see).
The way so far is kept here, a wrong first answer included.

**The serial boards had no such patch.**  The sync PROMs the model runs
(`firmware/d21_strv.r12`, D21, line timing; `d13_kdrv.r12`, D13, frame)
were read by Radon17 from a damaged НС4 (zx-pk.ru thread 15146 post
#274, 2024-10-20; rev. 3А, dated 03.07.91, «СТРВ»/«КДРВ» written on the
chips by hand).  His photo of that board shows D70 with the factory's
white wire on pin 11 and **nothing on pin 15**
([evidence/d70-oe-gnd/radon17-d70.jpg](evidence/d70-oe-gnd/radon17-d70.jpg)).
The leningrad.su board has no such wire either (above).  So Э3 is the
reference; the owner's patch is one board's, not to be kept as a
variant once Э3 works on the model (the owner, 2026-11-04).

**First answer, withdrawn: a stripe in 320 x 200.**  With К555ИР16 taking
MODE in the tick of the clock's fall, Э3's board showed the stale word at
the left edge of every 320 x 200 line (~15 samples a line), at both delay
corners, ROM-A or ROM-B.  The owner: games ran on those boards - "it
could not have failed to work".  Ruled out on the way: the ROM, the
delay corners, the PROM's pinout (КР556РТ12 = 82S136, confirmed pin for
pin against the MMI 63S440: 1 A6, 2 A5, 3 A4, 4 A3, 5 A0, 6 A1, 7 A2,
8 /E1, 9 GND, 10 /E2, 11 O4, 12 O3, 13 O2, 14 O1, 15 A9, 16 A8, 17 A7,
18 VCC; the netlist's D21 pins agree with Mick's redraw), the dump's
address and bit order (all four outputs read as clean timing waveforms
in this order), the PROM's other page (-01).

**The edge, tick by tick** (`run_board ... edges=FROM-TO`; a master tick
8.33 ns, 0 = D70 coming on; traces in
[evidence/d70-oe-gnd](evidence/d70-oe-gnd/), `trace-*.txt`).  The line's
load pulse - `~D71.8`/`СТРОБ`, the AND of ТС1..ТС4, on `ВК4` = MODE of
all four ИР16s - comes at -12..-4.  The clock (`ВК2`/`ВК3`: `ТС1` in
320, `ТС15 L` in 640) falls once inside it, at the very ТС15 edge that
steps the counter D30 and ends the pulse.  The border's end (`БОРДС L`
from D21) changes with ТС4 at that same edge, and gets to D70's enable
only through D30, D21, D14.1: D70 always comes on after the load pulse.
On Э3's board D70 is off in the border, `ВК2`..`ВК4` float high (`ВК2`
through R26), no clock runs, and the line's first load is lost.

**The fix to К555ИР16** (kept): the 74LS295 sheet (NTE74LS295A) takes the
inputs "one set-up time prior to" the clock's high-to-low edge; mode
control setup 20 ns, hold 0; data setup 20 ns, hold 10 ns; clock low 20
ns min., 30 MHz.  When D70 comes on, the clock and MODE leave the float
in the same tick: MODE was high through the setup time - **that edge
loads**.  `rtl/chips/k555ir16.sv` now takes MODE from the tick before
the edge (`sim/chips/test_k555ir16.cpp`, "MODE falling with the clock
loads").  Taking data and SER from the tick before as well broke 640 on
the owner's board (~47,000 samples) and was dropped; why was not traced.
The К555ИР16 sheet (eandc) has the same function table, no timing but
C to Q3 70/60 ns max.

**With it: 320 x 200 on Э3 is right** (`vid320`: one sample of the first
line, the first ~60 ns of the first point) - the clock's next fall is at
+11 on both boards, the late load is not seen.  **640 x 200 on Э3 is
not**: the clock falls every 8 ticks, so the load when D70 comes on is
one shift late - the first 16 points of every line one point to the
right, the 16th lost (`vid640`: 1305 samples a frame,
[evidence/d70-oe-gnd/model-640-left-edge.png](evidence/d70-oe-gnd/model-640-left-edge.png);
Omega's text, booted on both boards:
[evidence/d70-oe-gnd/model-omega-left-edge.png](evidence/d70-oe-gnd/model-omega-left-edge.png) -
«ОМ» one point to the right, the cursor a point longer).

**What 640 turns on: a race.**  Э3's board draws 640 right only if D70
comes on while `ТС15 L` is still high and at least MODE's setup before
its next fall: then `ВК4` falls alone, the next clock shifts, and the
word the registers took at the right border (D70 still on then for the
gates' delay after the load edge - the next line's first word; this is
how the old model got 640 right but for 7 samples) comes out in place.
Measured from the ТС15 edge that ends the load pulse, D70's enable takes
D30 (К555ИЕ10, 15.5 ns typ) + D21 (КР556РТ12) + the rise of `БОРДС L`
through R39 (1 kOhm) and C70 (100 pF) to the threshold (~25 ns) + D14.1
(9.5) + D70's enable (17); the window is ~52..66 ns.  On the model (RC
left out): РТ12 at 24..36 ns - right (`vid640` 7 samples, the first
word of the frame's first line); at 40 ns (82S136 typ., the table's
value) and 46 - wrong; with the RC's ~3 ticks added, wrong even at
12 ns.  So by the data sheets the serial boards should have lost the
race, and they did not: something in the model or in the reading of
the board is still off.  Being looked for; until found, nothing of this
goes into the tests as a property of the schematic.

**2026-11-08: three things were wrong in the model** - and the board
then drew the emulator's picture, but without C70, which the model had
left out (below, "C70 modelled"):

1. **The chips.**  The boards the plant made carry КР1533 where ПЭ3 says
   К555 (`netlist/fitted.csv`, the owner's 1990 board; `fitted-others.csv`,
   Frolov's and Radon17's of 1991), and on the edge's path D30 (ИЕ10) and
   D14 (ЛА3) are КР1533 on every board seen.  The generated boards now
   take them (`netlist/fitted-1991.csv`, the default of `tools/board.py`;
   КР1533ИЕ10 11.5 ns, КР1533ЛА3 6 - the midpoints of SN74ALS161B's and
   SN74ALS00A's min and max, no typical being printed).  D70 then comes
   on ~11.5 ns before `ВК2`'s fall at the typical delays.
2. **The К555ИР16's MODE setup.**  The sheet's 20 ns is a guaranteed
   maximum; the model's other delays are typical, and a typical path
   with a worst-case setup is a corner no chip has.  `SETUP_NS` in
   `tools/board.py` gives the register its setup by corner: the sheet's
   20 ns at max (2 ticks), MIN_SHARE of it at typ and min (1 tick, MODE
   from the tick before the edge) - the `SETUP` parameter of
   `rtl/chips/k555ir16.sv`.  With it the line's first clock shifts the
   word the registers took at the right border, as the design means: the
   left edge of every 640 x 200 line is the emulator's to the point.
   (A first attempt that day held all corners to 20 ns and lost the edge
   again; it took the typical corner for a worst one.)
3. **The frame's first word**, which was the board's, not an error: the
   registers load at each line's right edge the word after the line's
   last - the next line's first - and the last line's is word 8000, the
   first past the screen (the test ROM's word 8000 is 0B5C0, the `b5`
   the trace showed held).  Through the frame's border D70 is off and
   nothing loads, so in 640 x 200 the frame's top line starts with word
   8000; in 320 x 200 the line's first clock loads its own word as D70
   comes on, and only the frame's first sample, before that clock, shows
   word 8000's first point.  At the max corner (the sheet's 20 ns setup)
   the 320 x 200 line's first clock shifts as well, and the frame's first
   word has word 8000's points in word 0's attributes, the attribute
   registers loading on their own clock - a margin case, held to within
   one sample as the corners are.  `sim/board/screen_compare.h` (`held`) holds
   Э3's board to that; the emulator draws word 8000 there too (its
   `docs/hardware/video.md`, "The frame's first word").  Normally word
   8000 is as the screen's clear left it, and the corner looks like the
   rest - the serial machines' screens were clean.

**C70 modelled, 2026-11-09: 640 x 200 open again.**  C70 (100 pF on
`БОРДС L`) is on Э3 and fitted on Frolov's board; the model had it
"open".  It is now an "rc" part (`netlist/analog.csv`; off on the
owner's board, where it is bitten off): R39 (1 kOhm) charges C70 and
~25 pF of pins and wiring from the PROM's low (0.35 V) to D14.1's
threshold (1.4 V), the three LS inputs on the net adding their input
current - 23 ns later than without it (16 at min, 31 at max).  With it
320 x 200 stays right and 640 x 200 loses the edge again (`vid640` 1310
samples): D70 comes on 2 ticks after the clock's fall it has to
precede, the enable's own edge loads, and the line's first word is one
point late.  The ledger, from the ТС15 H rise that steps D30 to 0:
D70's enable at 11.5 (D30, КР1533ИЕ10) + 40 (D21, КР556РТ12) + 23 (the
RC) + 6 (D14, КР1533ЛА3) + 17 (D70's enable, К555КП11) = 97.5 ns; the
clock's fall it must precede at 66.7 + 9.5 (D31, К555ЛА4) + 9 (D70) =
85.2 ns - 12 ns short before any setup.  Looked at and not it: the
PROM (the Soviet tables give КР556РТ12 60 ns, the 82S136's maximum;
AMD's Am27S32, the same 1024 x 4 open-collector part, prints 38 ns
typical, 55 maximum, with 30 pF and its strong test pull-up, so the
40 ns typical stands, and the board's 1 kOhm pull-up is slower than the
test load, not faster); D70 (the К555КП11 sheet, eandc: data to output
18 ns, off to on 30 ns, both maximum - the model's 9 and 17 are already
about half); D70's inputs as drawn (pins 2/3, 5/6,
11/10, SEL, EO checked on Э3 again); the undriven MODE line read low
(640 x 200 407 samples, 320 x 200 broken). The `video: Э3's board
draws 640 x 200` test is may_fail again.

**The race by corner, with C70** (2026-11-09; `run_board 401 ...
rom=firmware/tests/vid640.rom drawn edges=400000-400200`, master ticks
of 8.33 ns from D14.3's fall, the step being the ТС15 H rise that ends
`СТРОБ`).  The line's first shift must come on the second ТС15 H rise
after the step; on the first, D70 is still off.

| corner | D70's outputs on | `ВК2` falls | which rise | picture |
|---|---|---|---|---|
| min | +1 (`ВК4` falls alone) | +5 | second: shifts | right |
| typ | +2 | +2, with `ВК4` | second: loads | first word a point late |
| max | +4 (`ВК4` falls alone) | +6 | third: shifts | first word a point late |

At max D70 comes on so late that it lands past the clock's low half,
in its next high half.  The first fall then shifts the held word, one
rise late.  The test passes there only because a corner compares with
one sample of slack (`screen_compare.h`, `slack`).  So with C70 Э3's
640 x 200 is right only when the chain from D30 to D70's enable is
about 20 ns faster than the typical figures, as the min corner's
(MIN_SHARE) is.

**The race in continuous time** (2026-11-09, ngspice from KiCad, a
one-off deck not kept).  To rule out the 8.33 ns grid, the race's nets
were simulated as analog nets, the chips' own delays taken as delays:
- `БОРДС L`: D21's open collector, R39 1 kOhm, C70, 15..35 pF of
  wiring, the inputs D73, D79 and D85 (LS: a Schottky diode from a 20 k
  base node clamped near 2 Vbe) and D14.1 (ALS: 50 k, near 3 Vbe).
- `ВК4`: four LS MODE inputs, floating while D70 is off (1.23 V).
- `ВК2`: R26 1 kOhm and two LS clock inputs.

Each driver's internal delay was calibrated to its typical figure on
the datasheet's test load:
- the PROM: Am27S32, 38 ns to 1.5 V with 300/600 Ohm and 30 pF, which
  gives 33 ns;
- the К555КП11: off to low 17 ns with 667 Ohm and 45 pF, giving 14.5;
  data 9 ns with 2 k and 15 pF, giving 7.6.

Times are from the step; ВК2's data fall is at 85.8 ns.

| C70 | wiring | БОРДС L at 1.4 V | ВК4 below 1.1 V | ВК2 below 1.1 V |
|---|---|---|---|---|
| none | 15..35 pF | 54..58 ns | 75..79 | 85.8 (the data's fall: shifts, 6..11 ns of MODE setup) |
| 100 pF | 15..35 pF | 77..82 | 98..103 | 100..105 (D70's enable itself: loads) |

So the grid is not it: in continuous time C70 adds ~23 ns, as computed,
and D70 comes on ~13..17 ns after the clock's fall it must precede.
Without C70 the setup MODE gets is 6..11 ns, under the sheet's 20.

On Э3, C70 is one of two such capacitors on D21's
open-collector outputs, beside C35 (68 pF on `СГИ L`).  Most likely
they filter out the PROM's glitches as its address changes; they are
not timing parts.

**Э3's board with the serial chain's speed** (2026-11-09, the owner's
choice).  Serial boards were made to Э3 with C70, and their screens
were clean, so their chain from D30's step to D70's enable was faster
than the typical figures: no part or wire found makes up the ~13 ns
otherwise.  `SERIAL_SPEED` in `tools/board.py` takes D30, D21, D14 and
D70 at 0.55 of the corner's delays on Э3's board (D70.15's switch); the
owner's board keeps them.  The share is **fitted, not measured**: the
middle of the race's window in continuous time (k ≤ 0.70 wins; at 0.55
MODE gets ~14 ns of setup; the sheet's 20 ns would need k ≤ 0.45).  With
it Э3's board draws 640 x 200 as the emulator at the typical corner (no
point different) and at min.  At max it does not: 0.55 of the max
figures is about the typical chain, which loses, and every line's first
word is a point off the rest (199 phase breaks, below) - first passed
there only by the corners' one sample of slack.  The window, ~33 ns, is
narrower than the corners' spread, so the test stays may_fail at them,
and fails at max.  What would prove it: a serial board's D21-to-D70 delays measured,
or a photo of a serial board's left edge at 640 x 200.

**The slack no longer hides the race** (2026-11-09).  One sample of
slack passes a first word a point late edge by edge: with the serial
speed off, the typical board's 1310 differing samples all passed within
one sample.  So `compare_screen` also tells the lines whose first word
lies at another shift than the rest of the line (`phase_breaks`, 640 x
200 only), and the picture tests want none, at every corner.  With the
serial speed off it finds 199 - every line but the frame's first, which
has the held word's rule; with it on, none at typ and min, and 199 at
max (so the Э3 640 test, may_fail, fails there).  At max, with the serial speed off, the
picture differed in 100 samples, each a line's first sample lit where
the border stands, and no phase break: one edge a sample early, of the
kind the owner's board shows at its right edge at max (100 samples at
639), not the first word's shift.

**C70 on the boards seen** (2026-11-04).  The owner's board has none:
two pads where the drawing puts it, by D85 towards D84, with C69 and
C73 fitted beside them - solder on both pads with the leads' stubs in
it: bitten off, not unsoldered (the owner, 2026-11-04), as
the bare wire on D70.15 would be the same hand's (the owner saw it on the photo;
[evidence/d70-oe-gnd/owner-c70-empty.jpg](evidence/d70-oe-gnd/owner-c70-empty.jpg)).
Frolov's board (leningrad.su, no D70 patch) has it: a yellow К10-17
there, unlike the blue 0.22 uF C69 and C73 - the 100 pF М47 of the
parts list
([evidence/d70-oe-gnd/frolov-c70.jpg](evidence/d70-oe-gnd/frolov-c70.jpg)).
Radon17's board has every capacitor of that half taken off - it cannot
tell.  So a serial board as Э3 draws it, C70 and all, drew 640 x 200
right; the RC cannot be what saves it, it only makes the model's race
worse - the error is elsewhere in the model.

**The right border** (`vid640`, Э3's board, the same trace taken where
D70 goes off): the load pulse at -12..-4 has a clock fall in it (-6,
loads the next line's first word); `БОРДС L` falls at -1 (pulled down by
the PROM, no RC on that edge), D70 goes off at 0 and the clock's next
fall (+2) is not seen.  So the registers wait out the border holding
that word unshifted, and the left edge is right only if D70's first
clock fall there is a shift at the counter's step after the load pulse -
D70 on, with MODE low, before that fall.  The two edges ask the same of
the path from D30 through D21 and D14.1 to D70's enable, but C70 slows
only the left one.  The owner's board had C70 bitten off and D70's
enable tied low: both speed that very edge up, which looks like one
repair of this left-edge fault on that board.  Whether the serial
boards showed it - some, by their chips' speed - or the model misses
something, is the open question.

**The frame's first line** (`vid640`, Э3's board, a frame's edges
traced; ticks from `БОРДК L` rising, a line 7680 ticks).  `БОРДК L`
rises at 0, inside the horizontal border (`БОРДС L` low from -1294 to
1266), so the line before the first picture line ran with `БОРДК L` low
all through: D70 off the whole line, no load at its right border.  D70
first comes on at 1267, the first picture line's left edge, and the
registers hold what they took at the right border of the previous
frame's last picture line.  Not a race: the margin is microseconds, any
board drawn as Э3 does it, the owner's (D70 always on) does not.  The 7
samples (`vid640`) and 11 (`vid320`) that Э3's board differs by from
the emulator even when the left edge's race is won are these, the first
word of the first line.

**Point 0 and `ДПСЛ`** (`vid640`, typical delays, edges= with ТС15,
`ДПСЛ` and the video output; ticks from the clock fall that loads the
line's first word on the owner's board).  `БОРДС L` gates the points
too: `ДПСЛ` = D85.2 = AND(~D49.10, `БОРДС L`, `БОРДК L`) goes through
D39 (ЛП5) to the video output, no register between.  D21 raises
`БОРДС L` with D30's step at the end of the load pulse, so the gate
opens while point 0 is already out.  On the owner's board (D70 always
on) point 0 is out from +1 to +9, the gate opens at +6 without C70: 3
ticks of its 8 seen (~25 of 67 ns); with C70 (+3 ticks of rise) it
opens at +9 and point 0 is not seen at all.  On Э3's board the gate
opens on the held word's point 0, D70's first fall reloads it, and point
0 shows ~11 ticks (~1.4 points) before points 1..14 come one point late
and point 15 is lost.  So no board as drawn shows point 0 whole, C70 or
not; with D70 always on C70 hides it outright - which is what biting C70
off on the owner's board mends.  The bench never saw it: it samples a
point once, at ТС15 H rising, late in the point, and the emulator draws
point 0 whole.  Read this way the owner's two changes are one repair of
the left edge - D70 always on for the shift, C70 off for the point the
first change leaves dark - and a one-point fault in the first 16
columns of 640 x 200 is small enough to have gone unseen on the serial
boards: whether they showed it is for the photos of working machines
([evidence/d70-left-edge](evidence/d70-left-edge/README.md)).

**The undriven lines** (2026-11-04).  While D70 is off nothing drives
`ВК2`..`ВК4`: `ВК2` has R26 to +5 V, `ВК3` (D55/D56's clock) and `ВК4`
(MODE of all four registers) have only LS inputs on them.  The model
reads an undriven net as 1.  The left edge rests on it.  In 320 x 200 the
clock is ТС1, low just after the word's step, so D70 coming on makes the
clock fall itself; with MODE still high (undriven) that fall is the
missed load, late, and the window is ~130 ns - the design works there
because the undriven MODE reads high.  In 640 x 200 the same
replacement needs D70 on within the first half point.  Э3's board, typical delays, `vid640` /
`vid320`, the samples that differ from the emulator (7 and 11 are the
first line's first word alone, i.e. right):

| `ВК3`, `ВК4` undriven read as | C70 | 640 x 200 | 320 x 200 |
|---|---|---|---|
| 1, 1 (the model) | no | 1305 | 11 |
| 1, 0 | no | 7 | 2453 |
| 1, 0 | yes | 412 | 2435 |
| 1, last driven level | no | 1305 | 11 |
| 1, last driven level | yes | 412 | 2435 |
| last, last | no | 7 | 707 |

No reading draws both modes right; in 640 x 200 only MODE read low (or
held) moves the left edge right.  Whether a board can read it so was
then computed, not guessed (a one-off integration of the net, not
kept; its model and figures follow).
An LS input is a Schottky diode under R1 (11..20 kOhm from +5 V, the
data sheets' I_IL 0.2..0.4 mA); left alone it sources current into the
net until R1's node reaches the two base-emitter drops it feeds, and
there the gate sees 1.  Four such inputs on 30..80 pF, D70's off-state
leakage pulling down: released low, the net reads 1 everywhere after
15..80 ns and settles at ~1.15 V; released high, it sinks no lower and
reads 1 throughout.  To read 0 the net needs ~1 mA taken out of it, a
1..2 kOhm path to ground no chip on it gives.  The pin sits near 1.1..1.4
V, close to the threshold in volts, but the reading is set by current,
and it is 1: the model's reading is the board's.  So the undriven lines
do not decide the left edge; the readings that fixed it are not
physical, and the open question is back to the ~40 ns of the race.  The
D70.15 on GND still removes the race itself, not an undriven line.

**The schematic read again, and the chips on the boards** (2026-11-05).
Looking for a misreading before blaming the boards:

- `ТС45 L` on D70.3/D70.6 is `ТС15 L`: in that hand a 1 has a flag that
  makes it look like a 4 (the ЛИ6 beside it is labelled «D7?.2» for
  D71), and sheet 3 makes no other `ТС..` net.
- E5 is a series link from the 15 MHz oscillator (D91.6) to `ТС15 H`,
  not a choice of phase.
- D21's dump: A0 (`ТС4`) gives the PROM half-word steps, and it uses
  them - O0 changes mid-word at word 60.  The picture's start is put on
  word 1's boundary on purpose: `БОРДС L` also opens the points (`ДПСЛ`),
  so raising it earlier would show eight points of whatever the
  registers hold.  No PROM content can fix the race; R26 on `ВК2` alone
  is there to make D70's enable itself the clock fall that loads the
  first word.  The owner says Xolod read the same contents from another
  board.
- The chips.  Э3 names functions only (ИЕ10, ЛА3, КП11); ПЭ3 says К555
  and lets К555 parts be fitted as КР1533.  The model took К555
  everywhere.  On the photos (Radon17's board, the owner's, Frolov's)
  the race's chain is КР1533: the video counters D29, D30, D36, D37, D42
  КР1533ИЕ10 (8910, 9001; Frolov's 9204), D14 КР1533ЛА3 (9002 on all
  three); D70 К555КП11, D71 К555ЛИ6 and the registers К555ИР16 as ПЭ3.
  Elsewhere on Radon17's board: D41 КР1533ТМ2, D57 КР1533ИР23, D64
  КР1533ЛН2, D91 КР1533ЛН1, D92/D94 КР1533ИР22, D97 КР1533ТМ9, and D63/D69
  КР1533КП12 - a different type from ПЭ3's К555КП2 (the ALS253's
  three-state outputs where the LS153 has strobes; D61, D62, D67, D68,
  D87 not on the photo).  D61, D62, D67, D68 and D87 have the strobes
  on GND, where the two are the same.  D63 and D69 have them on `ВК7 H`
  (D14.8, NAND of `СГИ L` and `КГИ L`: high through the sync pulses):
  there К555КП2 holds `СЦ`, `КЦ`, `ЗЦ` and ~D69.9 low, КР1533КП12 lets
  them go, and with only LS and 155 inputs on them (D75, D81) they read
  1 - the colour lines on through every sync pulse, the monitor's own
  blanking hiding it.  The machine works either way; the new board keeps
  ПЭ3's КП2, the model too
  ([evidence/d70-left-edge](evidence/d70-left-edge/README.md), pictures
  9 and 10).
- КР556РТ12: tA(A) 60 ns in the table of rfanat.qrz.ru («Микросхемы
  программируемых ПЗУ», table 1) - a maximum, as the 82S137's; the
  model's 40 ns typical stands.

TI prints the ALS parts' maximums only (SN74ALS161B SDAS276A p.8: CLK to
any Q 15/20 ns; SN74ALS00A SDAS187A p.3: 11/8 ns, both at 50 pF), so the
typical values below - 8 ns for the counters, 5 for the gate - are an
estimate, not a datasheet's.  Looked for again (2026-11-05), no typical
for the counter turned up anywhere: Philips' 74ALS161B (IC05, 1991 Feb
08, p.8) gives CP to Qn 4.0/6.0 ns min and 13.0/16.0 ns max (tPLH/tPHL,
50 pF, 0..70 C) and only a typical count rate, 140 MHz; the КР1533ИЕ10
sheet («Электроника и Связь», eandc.ru) gives pin 2 (C) to pins 11..14
at most 17 ns switching on and 15 ns switching off, maximums only, as
the КР1533ЛА3's 8 and 12 ns.  The one typical printed is the gate's:
Philips' 74ALS00A, 4.0 ns (its maximums 11/8 ns).  The estimate stands
between those bounds; nothing confirms or refutes it.  With them on D30, D42, D37 and D14 (Э3's
board, typical delays otherwise, samples differing from the emulator):

| `БОРДС L` rise added (ticks) | D21, ns | 640 x 200 | 320 x 200 |
|---|---|---|---|
| 0 (no C70) | 40 | 7 | 1 |
| 1 | 40 | 1305 | |
| 2 | 40 | 1306 | |
| 3 (C70 as computed) | 40 | 1307 | 1 |
| 3 | 30 | 1306 | |
| 3 | 20 | 107 | |

So with the chips the boards carry, Э3 without C70 draws both modes
right - by less than a tick (8.33 ns) - and with C70 loses the race
unless the PROM answers in ~20 ns, which no source gives it.  The
owner's board (the same КР1533) has C70 bitten off and D70 always on.
Frolov's has C70 fitted and, as far as its 980-pixel photo shows, no
D70 patch; if its 640 x 200 left edge is right, the chain there is
20..25 ns faster than the model, or C70 is not what the drawing says.
Asked on the forum; the model is not changed until then.

The delay corners agree with it (2026-11-06, the baseline's walk,
`docs/baseline.md`; the picture tests of Э3's board against the
emulator's picture, `build/cornerfix-*.log`).  At the min corner - every
chip at half its typical delay where no minimum is printed - Э3's board
draws both modes as the owner's does: off the emulator's picture in the
same 4222 and 100 samples as the owner's board there (the bench's
sampling, `docs/timing.md`, "Corners"), and in none within one sample.
At typ it is 1 sample in 320 x 200 and 1305 in 640 x 200; at max, within
one sample, none in 320 x 200 and 7 in 640 x 200.  So the model's Э3
loses the left edge at the typical К555 delays and not at half of them,
as with the ALS estimates above - a matter of how fast the chain's chips
are, which is the open question still.

So **Э3 is the patched board**: the copper had errors, the patches fix
them, and Э3 was drawn after.  This settles three of the six
"cannot tell from the scans" places (D16.1/D40.9, ~D30.15/СТРОБ, and the
`ИМП ЧТ` and `АПК7` splits) - in Э3's favour.  `vvv104.csv` loses its
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
shape only.  Both photos: [evidence/factory-patches](evidence/factory-patches/).

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

Where each of the nine stands (2026-10-20; closed 2026-11-03, below):

| Join in the traces | Link that joins | On the scans | Verdict |
|---|---|---|---|
| D65.2 `АД13` - D77.27 `Р3` | layer 8, D65.2 up to D77.27 | the run passes beside the ring of D77.27, a nub touches it | open - looks like the traces' error |
| D70.5 `ТС15 L` - D70.10 `~D71.8` | layer 2, via - D70.5 | no gold seen between the via and D70.5 (it may run under the tin) | **my misread of Э3**: pin 5 is on the D71.8 wire with pin 10 - Mick's redraw and the copper agree; fixed |
| D9.19 `АД01` - D1.6 `АД02` | layer 2, D9.19 - D1.6 | gold leaves D1.6 upward and goes under a layer-1 run, where it may turn | open - a short A0/A1 cannot be right; the schematic stands |
| D16.1 `~E1.2` - D40.9 `ТС8М L` | layers 7, 2, 8 through a square pad | every part of the chain is on the scans | copper differs from Э3, the factory patch D15.9-D40.9 (with a cut) restores Э3 |
| D86.11 `А13` - D9.6 `МК02`; D1.19 `УСТ` - D38.9 `СИНХР2` - D76.2 `ВКЛДВ`; D30.15 - D98.3 `СТРОБ`; D78.6 - D74.6 `А13`; D62.10 `АД06` - D79.9 `БОРДС` | long chains through vias on layers 2 and 7 | the inner runs are largely there; which via is wrong cannot be told - the scans show layers 2 and 7 only through the laminate | open - for another copy of the board or Mick's P-CAD file |

**All closed for Э3 (2026-11-03).**  Every join left open puts two
outputs on one wire, by the drivers `tools/board.py` resolves for each
net: `АД01`/`АД02` (D92.5, D92.6, both three-state on the bus), `А13`/`МК02`
(D74.6 against the bus's ten), `СИНХР2`/`УСТ` (D9.13 against D83.18),
`~D30.15`/`СТРОБ` (D30.15 against D71.6), `~D78.6`/`А13` (D78.6 against
D74.6), `АД06`/`БОРДС` (the open collector of D21.14 pulling the bus).
A board so joined would fight itself on nets the processor and the video
use every microsecond; the board built to Э3 runs the machine's software.
`АД13`/`Р3` would hang D94.15's bus bit on the 26 inputs `Р3` holds high
(D30, D37, D42's presets, the ROMs' PGM): the scans show the run passing
beside D77.27's ring, as said.  So the traces are wrong there, Э3 right,
and a board made from the netlist takes Э3's nets.  The 171 schematic
nets that come in pieces on the traces are runs the hidden layers
complete, as the joins above show the traces' limits; Э3 joins them.

How many layers the board has is not known.  The scans show four: each
side's own copper and the layer under it through the laminate - Mick
numbers them 1, 2 and 7, 8, which takes eight for granted; nothing read
says eight (his НС5 pictures «1+3» and «6+7» are of another board).
Ground and +5 V are all but absent from the four, so at least one more
layer carries them; the 171 nets in pieces, if they are not the traces'
errors, want hidden signal layers too.  Six or eight: the board's own
drawing, a cross-section of a hole or the edge of a real board, the
rev. 3 scans' finer look through the laminate, or Mick could tell.

Э3 sheet 5, D40 pin 13 (4D): a re-read took the short bar under it for a
join with the clock line of pin 9; it is a ground symbol, as the first
read and Mick's redraw (below) both have it.

## Э3 and the board: how disagreements are kept

Э3 was drawn by hand, the board may have changed after it, and the
documents we have come from unknown copies - neither source wins by rule.
`nets.csv` stays Э3 as read; a real board's differences live in
`netlist/<board>.csv` (`vvv104.csv` for the owner's НС4 rev. 3А, named after him - another
rev. 3А board, Frolov's, does not have its changes), one moved pin per row
with its evidence, and go in only when the copper or a photo shows them
beyond doubt.  `python tools/netlist.py check vvv104` applies and checks
them.  Where the drawing itself cannot be right - it contradicts the
technical description and stops the machine's software - the correction
lives apart again, in `netlist/deviations.csv`, in the same form, applied
over Э3 before the board's changes (none so far: R45, the first taken
for one, was a misread of the sheet).  The FPGA is built from Э3 with the rev. 3А changes, each one
switchable; where a change matters, software on the model decides.

In `vvv104.csv` so far: nothing - D40.9 was taken out again once the factory
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

The rest of the 20 lines, read on Э3 at full resolution on 2026-11-05
(the baseline's walk, `docs/baseline.md`; the crops:
[evidence/mick-differences](evidence/mick-differences/)) - all four
`nets.csv`'s, so none of the 20 is open:

- `ЗАП РЕГ H` at D79.3: Э3 sheet 2 prints «ЗАПРЕГ Н (Л2)» on the output
  of D79.1, the name the clocks of D78 and D97 carry; Mick's label there
  reads `L`, which leaves his registers without a clock;
- `ЗАДЕРЖ L`: Э3 prints «ЗАДЕРЖ L (Л3)» at D83.26 on sheet 2 and
  «ЗАДЕРЖ L (Л2)» at D98.9 on sheet 3 - one net by its name, as the
  cross-references say; in Mick's PDF the two pins come out on two nets
  with no name read (his label, or the reader's miss - not looked into:
  Э3 is plain);
- `РВК1 H` at E3.1: Э3 sheet 4 draws E3 from «РВК1 Н (Л4)» and E4 from
  «РВК0 Н (Л4)» to one ground sign; Mick has E3.1 on `АД05 H`;
- `ЗЦ H` at D81.10: the other input of the gate whose pin 9 is settled
  above (D81.9 on the D69.9 wire, by the copper); Mick has the two
  inputs of that one gate the other way round, which is the same gate.

## The 1991 placement off the assembly drawing (2026-11-10)

Every part but the DIPs now stands where the 1991 board had it
(`netlist/placement-1991.csv`): each label of the assembly drawing
3.858.420 СБ (page 27) read by eye (`netlist/sb-labels.csv`), the drawing
registered on Mick's holes, each part taken onto the pair of free holes
next to its label at its own pitch.  What it showed:

- **The pitches** are the grid's: the К10-17s 5.08 mm (2 steps), the
  С1-4 resistors, the diodes and the wire links 12.7 mm (5), the К53-14s
  25.4 mm, the КМ-6Б C72 10.16 mm, the LEDs 2.54 mm - 0.03 mm at most off
  across 190 parts.  Mick's traces draw the 1991 board on a 1.27 mm grid at
  2.54 mm a 39.4 pixels, as the board is laid out here; the plant's grid
  was 2.5 mm, so the board here is 1.6 % larger than the 1991 one, as it
  was before (`tools/pcb.py`, MM_PER_PX).  The real DIN socket XS2 keeps
  its 2.5 mm grid, 0.16 mm off its holes at most.
- **The drawing's "+"** on a diode is its pin 2 (the anode, KiCad's pad
  2): so on every diode whose copper reaches its net (VD1, VD2, VD5..VD8,
  VD10, VD11, VD13, VD14, VD17); on a tantalum it is pin 1, as C1's copper
  says.  VD3, VD4, VD12, VD15 and C2, C10, C11 are turned by it.
- **Two labels misread**, the nets saying which: "R81" is R58 (in series
  with VD11), "R82" R62 (on D92.1) - there is no R81 nor R82.
- **Against Э3: C4 - taken from the copper** (the owner, 2026-11-10).
  Э3 sheet 1 draws C4 (0.22 uF) with C10 and C9 from `-12V` to `GND`
  ([evidence/c4-rail](evidence/c4-rail/)).  On Mick's traces the
  `-12V` run joins exactly Э3's `-12V` pins - XS1 a31..c32, R15, R22,
  R23, VD1, VD2, C9, C10, XP2.39/40 - but not C4; C4's pad 1 is on a run
  of its own on layer 1 with D1.40 (the КР1818ВГ93's +12 V, the power
  table) and XP2.38 (`+12V`), its pad 2 on a plane.  So the copper has it
  as D1's +12 V decoupling, beside the chip as C23 and C31 are beside D20
  and D28, the board's two other +12 V chips; by Э3 D1 alone would have
  none.  The owner's board has a К10-17 in that place (photo `MC 0515
  04.jpg`; its tracks are not to be seen).  The model does not run the
  supplies, so no test tells; `nets.csv` has C4.1 on `+12V`, its note
  saying so.
- **Against Э3: XS1 c10.**  Э3 has `РЕЖ1 H` on XS1 pin 74 (c10); Mick's
  traces draw no hole there (the other 95 are drawn).  The pad is placed
  where the connector's geometry puts it.  Open, for a board.
- **Copper groups that merge two nets** (copper.json's reading, ten of
  them) gave two false alarms, set right by the other nets: C70's pad 1
  is on `БОРДС L` (its group also reaches `АД06 H`), XS1 b18 on `МК15 H`
  (its group reaches `АД01 H` and `АД02 H`).
- **Twelve parts turned round by the copper** (2026-11-10, laying Mick's
  outer layers).  The two-pin parts whose pins no copper net fixed took
  their order from the snapping; joined through the vias (the centre
  lines of `netlist/lines-1991.json`, which see vias `copper.json`
  misses), the copper put such a pin on its neighbour's net - E7's
  `~SA1.6` end through a via to R69's `GND` pin, where turning E7 and R69
  round makes it `ЧТМАГ H` both ends.  Turned round, as fewest copper
  groups with two nets want: C9, C72, C84, E7, E8, R22, R23, R66, R68,
  R69, R71, R73 (all unpolarised).  What is left with two nets is the
  copper read's merges, the factory patches' pins (D40.9, D98.3), E9's
  D80.5 - and XS2 and SA1, below.
- **XS2's numbering was the footprint's, SA1.8 a misread** (2026-11-10).
  The copper joins XS2's hole "1" of our hand-drawn footprint with
  SA1.15/16 (Э3's `~XS2.3`), hole "3" with SA1.19/20 (`~XS2.1`), hole
  "5" with SA1.13/14 (`~XS2.4`): Э3 and the copper agree, the footprint
  had pins 1 and 3, 4 and 5 the other way round along the row (the
  assembly drawing read from the wrong side).
  `kicad/ns4.pretty/DIN-5_Socket_Horizontal` renumbered so.  And SA1.8
  is `ЗАП МАГ H`, not `ВЫХКЛ L`: Э3 sheet 1 draws it from ЗАП МАГ Н's
  dot, a crooked 8 (the owner) - which the copper says too (SA1.8 with
  SA1.2 and C84.1), and the build variants want: switch 8 joins pins 8
  and 13, so the cassette build sends ЗАП МАГ out on XS2:4.  The model's
  RTL does not change (the sheet: `mc0515-ed` page 18, SA1's block).
- **What the copper still joins against Э3** is the eight places of
  `netlist/compare.txt` ("joined by copper"): the trace read's merges and
  the factory patches' pins - with E9's D80.5 and one more seen by the
  centre lines alone, D79.7 (`GND`) with D12.2 (`МК09 H`), a merge of the
  same kind.  Laid, each is cut where its nets meet.
