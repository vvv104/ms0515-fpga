#!/usr/bin/env python3
"""Find the DIP footprints among the pads of netlist/copper.json.

    python tools/footprints.py

A DIP is two horizontal rows of N/2 pads at the 2.5 mm pitch (39.4 px),
7.5 mm (118 px) or 15 mm (236 px) apart, pin 1 at the lower left (the
notch to the left, as the parts sit on the assembly drawing 3.858.420 СБ).

On НС4 the parts of a row stand end to end at one pitch, so their pad
rows run on without a break; the power pins split them.  The four layer
images show no plane, and the pins that go only to a plane are the ones no
drawn copper reaches: in every 7.5 mm part here the power pins are the
last of the lower row and the first of the upper one (7/14, 8/16, 9/18,
10/20, 4/8).  So a run of paired pads is cut into parts whose lower right
and upper left pads are such "plane" pads - the cut that covers the most
pads wins.  15 mm parts (ВВ51, ВВ55, ВИ53, ВГ93, the ROMs, the processor)
are taken where the run has exactly their length.
Writes netlist/footprints.json: pin count, the pad of every pin, the box.
"""
import json
import os

import numpy as np

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
PITCH = 39.4
NARROW = (118, (4, 7, 8, 9, 10))       # row distance, pins per row
WIDE = (236, (12, 14, 20))
TOL = 6


class Board:
    def __init__(self, copper):
        self.pads = np.array(copper["pads"])
        self.plane = np.ones(len(self.pads), dtype=bool)
        for net in copper["nets"]:
            self.plane[net] = False

    def at(self, x, y):
        d = np.abs(self.pads[:, 0] - x) + np.abs(self.pads[:, 1] - y)
        i = int(np.argmin(d))
        return i if d[i] < TOL else None

    def runs(self, span):
        """Chains of (lower, upper) pad pairs at the pin pitch."""
        pairs = {}
        for p, (x, y) in enumerate(self.pads):
            q = self.at(x, y - span)
            if q is not None:
                pairs[p] = q
        starts = [p for p in pairs if self.at(self.pads[p][0] - PITCH, self.pads[p][1])
                  not in pairs]
        out = []
        for p in starts:
            run = [(p, pairs[p])]
            while True:
                x, y = self.pads[run[-1][0]]
                n = self.at(x + PITCH, y)
                if n is None or n not in pairs:
                    break
                run.append((n, pairs[n]))
            out.append(run)
        return out


def make(board, run, i, h):
    lower = [run[k][0] for k in range(i, i + h)]
    upper = [run[k][1] for k in range(i, i + h)]
    pins = lower + list(reversed(upper))
    xs = [float(board.pads[p][0]) for p in pins]
    ys = [float(board.pads[p][1]) for p in pins]
    return {"pins": 2 * h, "pads": [int(p) for p in pins],
            "box": [min(xs), min(ys), max(xs), max(ys)]}


def cut(board, run, lengths):
    """Best split of a narrow run into parts bounded by plane pads."""
    n = len(run)
    best = [(0, [])] * (n + 1)
    for i in range(n - 1, -1, -1):
        best[i] = best[i + 1]
        for h in lengths:
            if i + h > n:
                continue
            if not (board.plane[run[i + h - 1][0]] and board.plane[run[i][1]]):
                continue
            score = best[i + h][0] + 10 * h - 1
            if score > best[i][0]:
                best[i] = (score, [(i, h)] + best[i + h][1])
    return [make(board, run, i, h) for i, h in best[0][1]]


def main():
    with open(os.path.join(ROOT, "netlist", "copper.json"), encoding="utf-8") as f:
        board = Board(json.load(f))
    dips, used = [], set()
    for run in board.runs(WIDE[0]):
        if len(run) in WIDE[1]:
            dips.append(make(board, run, 0, len(run)))
            used.update(dips[-1]["pads"])
    for run in board.runs(NARROW[0]):
        for d in cut(board, run, NARROW[1]):
            if not used.intersection(d["pads"]):
                dips.append(d)
                used.update(d["pads"])
    dips.sort(key=lambda d: (d["box"][1], d["box"][0]))
    with open(os.path.join(ROOT, "netlist", "footprints.json"), "w", encoding="utf-8") as f:
        json.dump(dips, f, indent=0)
    count = {}
    for d in dips:
        count[d["pins"]] = count.get(d["pins"], 0) + 1
    print(len(dips), "DIPs:", dict(sorted(count.items())))


if __name__ == "__main__":
    main()
