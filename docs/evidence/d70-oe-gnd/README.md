# D70's OE on GND (the owner's board)

The case is told in [findings.md](../../findings.md), section «The factory
wire patches restore Э3», the paragraphs on the thin bare wire; the change
is `netlist/vvv104.csv` (D70.15 off `~D14.3`, on GND).

| File | What |
|---|---|
| `owner-d70.jpg` | the owner's board (`photos/known-machines/MC 0515 04.jpg`), three times enlarged: К555КП11 (D70) right of the resistors R24, R25, R26; the thin bare wire runs under the resistors from the corner pin of the ИР23 on the left (D57 pin 10, GND) to D70's pin row; the white wire beside it is the factory patch D98.3 - D70.11 |
| `bare-wire-labelled.jpg` | the bare wire four times enlarged, its ends ringed: D57.10 (the top pin of the ИР23's right row, GND) and D70.15 (the КП11's left row, second from the bottom - the pin lifted off its pad); D70.11 above it is where the factory's white wire lands. The D57.10 ring sits about half a pin left of where the wire starts - the photo's resolution |
| `owner-d70-labelled.jpg` | the same corner on the labelled crop of the photo (`annotated-MC 0515 04-wires.jpg`) |
| `drawing-d70.jpg` | the assembly drawing 3.858.420 СБ (page 27, turned half round) at D70: the patch to D70.11 is drawn, a wire to D70.15 is not - a later fix |

That D70's pin 15 is lifted off its pad the owner saw on the photo
(2026-10-20); it cannot be made out at that size.  The full-size close-up
of 2015 shows it:

| File | What |
|---|---|
| `closeup-d70-pin15.jpg` | `photos/known-machines/16337efabef4.jpg` (2560 px, the owner's board in 2015, turned half round: the chip's notch on the right), twice enlarged: К555КП11 (D70), R24..R26 below it; the bare wire comes up past the resistors' right ends and is soldered to D70's pin 15 (the row nearest the resistors, counted from the left 9, 10, ... - the seventh), which stands bent up off the row; the white wire along the same row is the factory patch to D70.11 |
| `closeup-bare-wire-whole.jpg` | the same photo, the whole bare wire: from D70's lifted pin 15 straight down past the right ends of R24..R26 to the КР1533ИР23 (date 9010) below, onto the left end of its upper row - turned half round as it is (its notch on the right), pin 10, GND: D57.10 to D70.15, as the owner read it on 2026-10-20 |


What the change does is told in findings.md, the same section, «What it
does, on the model» - still open (2026-11-04): on the model Э3's board
draws 640 x 200 wrong at the left edge, the serial machines did not.

| File | What |
|---|---|
| `radon17-d70.jpg` | Radon17's damaged НС4 (rev. 3А; zx-pk.ru thread 15146 post #274, 2024-10-20 - the board the sync PROMs `firmware/d13_kdrv.r12`, `d21_strv.r12` were read from), cropped at D70: К555КП11 with R24..R26 below it, the factory's white wire on pin 11, nothing on pin 15 - a serial board as Э3 draws it |
| `trace-320-owner.txt`, `trace-320-drawn.txt`, `trace-640-owner.txt`, `trace-640-drawn.txt` | `run_board ... edges=` on `vid320`/`vid640`, the owner's board and Э3's (`drawn`): the nets at the picture's left edge tick by tick, 0 = D70 coming on. The load pulse (`СТРОБ`, `ВК4`) at -12..-4 has a clock fall inside it on the owner's board only; on Э3's the clocks and MODE leave the float together at +2 |
| `model-640-left-edge.png` | `vid640`, the picture's left edge twelve times enlarged, the owner's board above the red bar, Э3's below (К555ИР16 as the data sheet has it): the first 16 points one point to the right |
| `model-omega-left-edge.png` | Omega booted on both boards (disk `test_vvv_system.dsk`, 12 s, frame 700), its 640 x 200 text at the left edge twelve times enlarged, the red lines at points 0, 16, 32, 48: on Э3's board (below) «ОМ» one point to the right, the О's left stroke a point wider, the cursor a point longer - what the model makes of Э3, which the serial machines did not show |
| `owner-c70-empty.jpg` | `green-wire-ad03/owner-green-wire.jpg` twice enlarged, the chips named by `netlist/placement.csv` (each type agrees with its print): the place of C70 (100 pF on `БОРДС L`) by D85 is two bare pads, ringed red; C69 and C73 beside it are fitted (both names with a question mark - read off the drawing's places), as is R39 under the green wire's end |
| `frolov-c70.jpg` | Frolov's board (`photos/known-machines/mc0515-6.jpg`, leningrad.su), four times enlarged at D80, D85, D87: C70 fitted, ringed - yellow, unlike the blue 0.22 uF C69 and C73 |
| `model-left-edge.png` | the OLD model (MODE taken after the edge): the 320 x 200 stripe on the power-on screen that the 74LS295's timing does away with - kept as the record of the mistake |
