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


def resample(spans, step_ms):
    """For a film of a fixed step (a GIF: 20 ms, 50 a second - its delays
    are in hundredths and players stretch anything shorter): which frame is
    on the screen at each step of the machine's clock."""
    out, k = [], 0
    start, end = spans[0][0], spans[-1][1]
    t = start
    while t < end:
        while k + 1 < len(spans) and spans[k][1] <= t:
            k += 1
        out.append(k)
        t += step_ms * TICKS_PER_MS
    return out


def canvas_height(heights):
    """The film's height: the frames' most common one.  A film's frames
    are one size; the few after power-on, before the sync settles, are cut
    or filled out to it, as a monitor would show them."""
    return max(set(heights), key=heights.count)


def main(argv):
    from PIL import Image

    folder = argv[1]
    film = argv[2] if len(argv) > 2 else os.path.join(folder, "film.webp")
    rows = []
    with open(os.path.join(folder, "frames.txt"), encoding="ascii") as f:
        for line in f:
            n, start, end = (int(v) for v in line.split())
            rows.append((n, start, end))
    images = [Image.open(os.path.join(folder, f"{n:05d}.bmp")).convert("RGB") for n, _, _ in rows]
    width = max(i.width for i in images)
    height = canvas_height([i.height for i in images])
    frames = []
    for image in images:
        canvas = Image.new("RGB", (width, height))
        canvas.paste(image.crop((0, 0, width, min(height, image.height))), (0, 0))
        frames.append(canvas.resize((width, height * 2), Image.NEAREST))
    spans = [(s, e) for _, s, e in rows]
    if film.lower().endswith(".gif"):
        steps = resample(spans, 20)
        shots = [frames[k] for k in steps]
        shots[0].save(film, save_all=True, append_images=shots[1:], duration=20, loop=0, optimize=False)
        print(f"{film}: {len(shots)} steps of 20 ms, {len(shots) * 0.02:.2f} s of the machine")
        return 0
    ms = durations(spans)
    frames[0].save(film, save_all=True, append_images=frames[1:], duration=ms, loop=0, lossless=True)
    print(f"{film}: {len(frames)} frames, {sum(ms) / 1000:.2f} s of the machine")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
