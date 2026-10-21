# The bench

The board (`rtl/board/ns4.sv`) is all the model there is of the machine;
what is outside it - the supply, the drive, the keyboard - the bench
plays, in `sim/board/`, at the board's connectors.

| Part | Where | What it does |
|---|---|---|
| The supply | `power.h` | holds the supply monitor VD11/R58 low for the first millisecond, as the supply rises, so D16 starts from a cleared counter (`docs/analog.md`); `disconnect()` sets every board input off - nothing on the connectors |
| The drive | `drive.h`, `xp4.h` | the «Электроника МС 5305» on XP4 by the cable 4.854.127: the spindle, index, READY, head and read pulses; the tracks laid out as the MS 0515's own formatters write them (ОМЕГА, vvv, FDZ); writing is still to come |
| The keyboard | `keyboard_stub.h`, `xp4.h` | a stand-in for the МС 7004 on XS2 that answers ROM-B's ID request (AB) with 01 00, so ROM-B finds the keyboard ready rather than waiting some 25 s of the machine; the keyboard's own model is to come |
| Snapshots | `snapshot.h`, `snapshot_file.h` | the board's state (Verilator `--savable`) and the bench's parts in one file, so a run goes on from where an earlier one stopped |

## Runs

`run_board <machine ms> <folder> [every Nth frame] [video] [seed=N]
[disk=IMAGE] [layout=omega|vvv|fdz] [keyboard] [save=FILE@MS] [load=FILE]`

- the processor's fetches against the emulator's (from power-on only: a
  run from a snapshot is not followed), every Nth frame against its
  screen, point for point;
- `seed=N`: a random power-up state; `video`: every frame, for
  `tools/frames_to_video.py`;
- `save=FILE@MS` / `load=FILE`: a snapshot at machine time MS, a run
  from one to <machine ms>.  Pass the same disk and keyboard to the run
  that goes on (the drive's state comes from the snapshot, its image too).

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
