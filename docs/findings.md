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
