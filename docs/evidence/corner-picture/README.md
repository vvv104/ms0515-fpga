# The picture at a corner of the delays: one sample at the edges

The case is told in [timing.md](../../timing.md), "Corners": the picture
tests fail at the min corner by single columns of samples, because the
bench samples the picture at the rises of ТС15 H and the board's outputs
to the video DAC do not change together.

| File | What |
|---|---|
| `min-against-typ-320.png` | `vid320.rom`'s frame 25 at typ, the samples that differ at min in red, twice enlarged across and three times down, the picture's upper left: the border's edge, the picture's edge and every 16th sample - a word's boundary |

The picture was made by comparing, sample by sample, the frames of
`run_board 420 FOLDER 1 rom=firmware/tests/vid320.rom` from
`build/Release/sim` and `build/min/sim`; the table of timing.md counts,
in the edge trace of `run_board 408 FOLDER 1000
rom=firmware/tests/vid320.rom edges=400000-406000`, how many master ticks
after a rise of ТС15 H each change of the picture's outputs, of the
attribute registers and of ДПСЛ comes.  Both were one-off scripts, not
kept.
