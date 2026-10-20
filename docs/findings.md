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
