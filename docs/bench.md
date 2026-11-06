# The bench

The board (`rtl/board/ns4.sv`) is all the model there is of the machine;
what is outside it - the supply, the drive, the keyboard - the bench
plays, in `sim/board/`, at the board's connectors.

| Part | Where | What it does |
|---|---|---|
| The supply | `power.h` | holds the supply monitor VD11/R58 low for the first millisecond, as the supply rises, so D16 starts from a cleared counter (`docs/analog.md`); `disconnect()` sets every board input off - nothing on the connectors |
| The drive | `drive.h`, `xp4.h` | the «Электроника МС 5305» on XP4 by the cable 4.854.127: the spindle, index, READY, head and read pulses; the tracks laid out as the MS 0515's own formatters write them (ОМЕГА, vvv, FDZ); what the controller writes goes into the image's sectors |
| The keyboard | `keyboard_stub.h`, `xp4.h` | a stand-in for the МС 7004 on XS2 that answers ROM-B's ID request (AB) with 01 00, so ROM-B finds the keyboard ready rather than waiting some 25 s of the machine; the keyboard's own model is to come |
| Snapshots | `snapshot.h`, `snapshot_file.h` | the board's state (Verilator `--savable`) and the bench's parts in one file, so a run goes on from where an earlier one stopped |

## Runs

`run_board <machine ms> <folder> [every Nth frame] [video] [seed=N]
[disk=IMAGE] [layout=omega|vvv|fdz] [drive=logic] [keyboard] [save=FILE@MS] [load=FILE]
[rom=IMAGE] [keys=FILE] [watch=LO-HI] [spot=ROW:COLUMN] [irq] [fetches=FROM-TO]
[drawn] [edges=FROM-TO]`

- the processor's fetches against the emulator's (from power-on only: a
  run from a snapshot is not followed), every Nth frame against its
  screen, point for point;
- `drive=logic` (with `disk=`): the MiSTer core's drive
  (`rtl/parts/mc5305.sv`) on XP4 in place of the bench's, the image served
  through MiSTer's block interface (`sim/board/logic_drive.h`), the
  bench's drive beside it on the same lines as the reference: the run
  ends saying where their outputs differed and whether the two images
  ended the same.  By design they differ only while the logic loads a
  track after a step, a side's change or the motor's start: the bench's
  drive gives its flux pulses there and the logic none yet, so each
  stretch of difference is one pulse, 120 ticks.  And they may for a few
  bytes after a write: the bench's drive shows the new sector as the
  gate goes off and the logic from its next field, so if the write ends
  before the layout's field does, or right at its CRC's last cell, the
  cells up to the first of the gap after it can differ (nothing reads
  there).  The ticks within four bytes after a write's end are counted,
  and apart from them the ticks that differ later than 3 ms after a
  head's move - by design none.  Where the two images differ the blocks are listed, each with
  what the logic's drive and the bench's changed in it since the image
  was mounted.  From power-on only: the logic's state is not in
  a snapshot, and a drive started afresh under a controller that has
  already seen one (its head stepped, its disk turning) does not boot;
- `seed=N`: a random power-up state; `video`: every frame, for
  `tools/frames_to_video.py`;
- `save=FILE@MS` / `load=FILE`: a snapshot at machine time MS, a run
  from one to <machine ms>.  Pass the same disk and keyboard to the run
  that goes on (the drive's state comes from the snapshot, its image too);
- `rom=IMAGE`: another ROM pair in the sockets XS3/XS4 - the 16 KB image
  written into D65/D77's cells before power-on (`sim/board/rom_socket.h`),
  the emulator given the same; ROM-A is `firmware/ms0515-roma.rom`.  A
  snapshot carries the ROM it was taken with;
- `keys=FILE` (with `keyboard`): the keys the stub presses - a line
  `MS CODE CODE ...`, the machine time in ms and the МС 7004's codes in
  octal (LK201 make codes: a letter alone, СУ 257 before a control key,
  ALL-UP 263 when a modifier is let go), sent one after another; `#`
  begins a comment.  From a snapshot, the lines before its time are passed
  over;
- `watch=LO-HI` (octal, may repeat): every write of the processor to
  those addresses - the time, the address, word or byte, the data, and
  the DRAM cell (row:column, octal) it went to, if it went to the DRAM;
- `spot=ROW:COLUMN` (octal, may repeat): where on the screen the video
  reads that DRAM cell - the frame's line and sample, once a frame.
  With `watch` it tells where a write shows up (it found R45,
  `docs/findings.md`);
- `irq`: the interrupt requests at D89's inputs (`ЗАПР КЛ`, `ТАЙМ`,
  `М С2`, `Д С2`, `7007`, `МОН`) each time they change, with the code D96
  puts on CP0..CP3; each interrupt acknowledge (a row strobe with SEL1
  high and SEL0 low) and the handler's first fetch; every 20 ms the last
  fetch.  `fetches=FROM-TO` (machine microseconds) adds every fetch in
  that stretch and implies `irq` (it found how OSA's ^Q sticks,
  `docs/findings.md`).
- `drawn`: the board as Э3 draws it, without the owner's board's changes
  (`netlist/vvv104.csv`: D70.15 on `~D14.3`, not on GND);
- `edges=FROM-TO` (machine microseconds): every change of the nets at the
  picture's left edge, master tick by tick - the border (`БОРДС`,
  `БОРДК`), D70's enable (`~D14.3`), the strobes `ВК2`..`ВК5`, `СТРОБ`,
  `ЧТ ВЗУ`, `ССИ`, `РАЗР`, as lines "edges: TICK b0 .. b10 AA DD T P V"
  (the D70 case, `docs/findings.md`); AA the attribute registers'
  outputs in hex (D56 QA..QD = `СН КР ЗЛ СНФ` the low nibble, D55 QA..QD
  = `КРФ ЗЛФ ИНТ БЛНК` the high), DD the video data's high byte
  `ДВ08`..`ДВ15` they load, T the dot clock `ТС15 H`, P the gated point
  `ДПСЛ H`, V the video output as the frames take it (blue 1, red 2,
  green 4, brightness 8) - between the frames' samples.

Every run also watches the video's words (`sim/board/video_watch.h`) and
ends with three "video words" lines and a "video requests" one:

- what the latches D51, D57 take at ЧТ ВЗУ H against the DRAM cell the
  video read (the DRAM watch's shadow; from a snapshot the shadow is
  empty and nothing is checked), what the К555ИР16s load at ВК2/ВК3
  against what the latches took;
- loads of a word loaded before (no ЧТ ВЗУ since: the word on the screen
  twice) and words latched but never loaded;
- video reads of another cell than the address counters named, as the
  multiplexers D61, D62, D67, D68 put them on А0..7;
- the video's requests: СТРОБ H strobes that came while ЧТ ВЗУ still held
  the request flip-flop D98.1 reset (each a word not read), and how close
  a strobe came after the reset ended (`docs/timing.md`, "Corners").

The first findings of each kind each machine second are printed, the
first few with the nets around them tick by tick.

`tools/run_many.py` runs several side by side, a power-up state each.

## Times

At about 250 times slower than the machine, a second of it is four
minutes.  ROM-B's self-test takes some 6 s of the machine; with the
keyboard stub the drive is read from about 7 s; Omega boots in the
emulator by about 10 s after that.  Long runs go on by themselves
(`Start-Process`, not a shell's background job, whose time limit would
end them) - with the computer kept out of standby, which throttles a
process it does not stop.  Snapshots taken at the stages - after the
self-test, at the OS's prompt - let every later run start there.
