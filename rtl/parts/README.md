# The bench's parts as logic

What the C++ bench plays around the board (`sim/board/`) - the floppy drive,
the keyboard, the camera on the video DAC's inputs - becomes logic here for
the MiSTer core (plan stage 7c).  These are not chips of the board: their
ports are the board's nets on one side and MiSTer's interfaces on the
other, all on the master clock.  Each has its test in `sim/parts/` and is
checked on the board's own levels against the bench part it replaces
(`sim/board/test_board_*.cpp`).

- `ns4_video.sv`: the picture - the levels the camera takes at each ТС15 H
  rise, out a line later with a pixel enable, the syncs and the line and
  frame blankings apart (the board ties the two blankings on one wire).
