# Open reads

Places of Э3 not yet read with certainty.  Each is closed by another place
of the drawing, by the board traces, or by the user; then it moves into
`nets.csv` and leaves this list.

| Sheet | Where | What is unsure |
|---|---|---|
| 2 | D97 Q0 (pin 2) | net name read as `РПРРТ Н` |
| 3 | D98.1 (ТМ2 section 1) | the inverted output is drawn as pin `8`, which is section 2's; section 1's is pin 6 - a stub either way, not entered |
| 4 | D81.3 pin 9 and D69 pin 9 (`2Y`) | the wire from D69.9 seems to join the `ДПСЛ Н` vertical that feeds D81 pins 1 and 4 - two outputs on one net? - question q3 to the user |
| 4 | D21 pin 17 (A7) | label read as `РВК0 Н`, the same as D13's A9 - to check |
| 5 | D76 PB5 (pin 23) `ОБСЛ Н`, PB6 (pin 24) `ЗАВЕРШ Н` | two arrows `ХР3:12` and `ХР3:11` - which line goes to which contact - question q4 to the user |
| 1 | XS3..XS6 | the DIP sockets under D65, D77 (28 pins) and D1, D83 (40 pins): pin k of the socket is pin k of its chip; not entered as nets |
