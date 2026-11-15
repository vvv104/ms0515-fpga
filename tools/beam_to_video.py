#!/usr/bin/env python3
"""Draw run_board's frames as the monitor's beam draws them.

    python tools/beam_to_video.py <folder>/beam [film.mp4|film.webp]
           [--per N] [--ticks FROM-TO] [--still N]

run_board ... beam writes each frame as the master ticks the video DAC's
digital inputs change at (sim/board/beam.h) and beam/frames.txt, a line a
frame: its number and the master ticks it began and ended at.  No point is
sampled here: a column is `per` master ticks of the line (4 by default,
half a 640 x 200 point) and its colour the mean of the light those ticks
gave, as the phosphor sums it - a point held three ticks of its eight shows
narrow and dim, not whole and not gone.  Bright 255, not bright 170 a gun,
as the frames run_board saves.  Lines are doubled.  --ticks FROM-TO keeps
that stretch of each line (an edge at --per 1); --still N writes frame N
alone as a PNG beside the film.  An .mp4 (OpenCV) plays at the frames' mean
rate; a .webp (Pillow) keeps each frame's own length.
"""
import argparse
import os
import struct
import sys

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from frames_to_video import TICKS_PER_MS, durations  # noqa: E402

LINE_TICKS = 7680       # 64 us of 120 master ticks


def palette():
    """Each 4-bit colour's light, R, G, B: blue 1, red 2, green 4, bright 8."""
    out = np.zeros((16, 3), dtype=np.float64)
    for c in range(16):
        on = 255.0 if c & 8 else 170.0
        out[c] = ((c & 2) and on, (c & 4) and on, (c & 1) and on)
    return out


def read_frame(data):
    """A .beam file's lines: each a list of (tick, colour)."""
    at = 0

    def u16():
        nonlocal at
        (v,) = struct.unpack_from("<H", data, at)
        at += 2
        return v

    lines = []
    for _ in range(u16()):
        line = []
        for _ in range(u16()):
            tick = u16()
            line.append((tick, data[at]))
            at += 1
        lines.append(line)
    return lines


def line_ticks(changes, width=LINE_TICKS):
    """The colour at each of the line's ticks: each change held to the next."""
    out = np.zeros(width, dtype=np.uint8)
    for k, (tick, colour) in enumerate(changes):
        if tick >= width:
            break
        end = changes[k + 1][0] if k + 1 < len(changes) else width
        out[tick:min(end, width)] = colour
    return out


def draw(lines, per=4, first=0, last=LINE_TICKS, height=None):
    """The frame as an RGB array: a column per `per` ticks of first..last,
    its light the ticks' mean; lines doubled; cut or filled to `height`
    lines (a film's frames are one size)."""
    light = palette()
    span = (last - first) // per * per
    rows = height if height is not None else len(lines)
    out = np.zeros((rows, span // per, 3), dtype=np.float64)
    for y, changes in enumerate(lines[:rows]):
        ticks = line_ticks(changes, last)[first:first + span]
        out[y] = light[ticks].reshape(-1, per, 3).mean(axis=1)
    return np.repeat(np.rint(out).astype(np.uint8), 2, axis=0)


def common_height(counts):
    """The frames' most common line count (those after power-on, before
    the sync settles, are cut or filled to it, as a monitor shows them)."""
    return max(set(counts), key=counts.count)


def write_mp4(path, images, spans):
    import cv2

    seconds = (spans[-1][1] - spans[0][0]) / (TICKS_PER_MS * 1000)
    fps = len(images) / seconds
    h, w = images[0].shape[:2]
    out = cv2.VideoWriter(path, cv2.VideoWriter_fourcc(*"mp4v"), fps, (w, h))
    for image in images:
        out.write(image[:, :, ::-1])
    out.release()
    print(f"{path}: {len(images)} frames at {fps:.3f} a second, {seconds:.2f} s of the machine")


def write_webp(path, images, spans):
    from PIL import Image

    frames = [Image.fromarray(i) for i in images]
    ms = durations(spans)
    frames[0].save(path, save_all=True, append_images=frames[1:], duration=ms, loop=0, lossless=True)
    print(f"{path}: {len(frames)} frames, {sum(ms) / 1000:.2f} s of the machine")


def main(argv):
    p = argparse.ArgumentParser()
    p.add_argument("folder")
    p.add_argument("film", nargs="?")
    p.add_argument("--per", type=int, default=4)
    p.add_argument("--ticks", default=f"0-{LINE_TICKS}")
    p.add_argument("--still", type=int)
    a = p.parse_args(argv[1:])
    first, last = (int(v) for v in a.ticks.split("-"))
    rows = []
    with open(os.path.join(a.folder, "frames.txt"), encoding="ascii") as f:
        for line in f:
            n, start, end = (int(v) for v in line.split())
            rows.append((n, start, end))
    frames = []
    for n, _, _ in rows:
        with open(os.path.join(a.folder, f"{n:05d}.beam"), "rb") as f:
            frames.append(read_frame(f.read()))
    height = common_height([len(f) for f in frames])
    if a.still is not None:
        from PIL import Image

        k = [n for n, _, _ in rows].index(a.still)
        path = os.path.join(a.folder, f"still{a.still:05d}.png")
        Image.fromarray(draw(frames[k], a.per, first, last)).save(path)
        print(path)
        if a.film is None:
            return 0
    film = a.film or os.path.join(a.folder, "film.mp4")
    images = [draw(f, a.per, first, last, height) for f in frames]
    spans = [(s, e) for _, s, e in rows]
    (write_webp if film.lower().endswith(".webp") else write_mp4)(film, images, spans)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
