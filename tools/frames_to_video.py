#!/usr/bin/env python3
"""Make run_board's frames a film in the machine's own time.

    python tools/frames_to_video.py <folder>/video [film.webp]

run_board ... video writes every frame of the board's screen as a BMP and
video/frames.txt, a line a frame: its number, the master tick it began at
and the one it ended at.  Each frame shows for as long as it lasted on the
machine (120 master ticks a microsecond), so the film plays in real time,
not at the simulation's pace.  An animated WebP (Pillow): its durations
are whole milliseconds, kept to the machine's clock without drifting.
Lines are doubled, as a monitor's interlace-free raster looks.
"""
import os
import sys

TICKS_PER_MS = 120_000


def durations(spans):
    """Whole milliseconds for frames (start tick, end tick): each rounded
    where the machine's clock has got to, so the sum never drifts."""
    out, shown, clock = [], 0, 0.0
    for start, end in spans:
        clock += (end - start) / TICKS_PER_MS
        ms = int(clock + 0.5) - shown
        out.append(ms)
        shown += ms
    return out


def main(argv):
    from PIL import Image

    folder = argv[1]
    film = argv[2] if len(argv) > 2 else os.path.join(folder, "film.webp")
    rows = []
    with open(os.path.join(folder, "frames.txt"), encoding="ascii") as f:
        for line in f:
            n, start, end = (int(v) for v in line.split())
            rows.append((n, start, end))
    frames = []
    for n, _, _ in rows:
        image = Image.open(os.path.join(folder, f"{n:05d}.bmp")).convert("RGB")
        frames.append(image.resize((image.width, image.height * 2), Image.NEAREST))
    ms = durations([(s, e) for _, s, e in rows])
    frames[0].save(film, save_all=True, append_images=frames[1:], duration=ms, loop=0, lossless=True)
    print(f"{film}: {len(frames)} frames, {sum(ms) / 1000:.2f} s of the machine")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
