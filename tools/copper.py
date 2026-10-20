#!/usr/bin/env python3
"""Read the copper of НС4 out of Mick's layer traces.

    python tools/copper.py [LAYER_DIR]

LAYER_DIR  the folder with NS4-1.gif, NS4-2.gif, NS4-7.gif, NS4-8.gif
           (default: $MS0515_DATA/scans/boards/radon17-layers, MS0515_DATA
           falling back to ../ms0515_data beside the repository)

Writes netlist/copper.json: every plated hole ("pad", its centre in the
layer images' pixels, 4488x3543, the component side) and the groups of
pads one copper area joins on any of the four layers.

How the images are read: each layer is drawn on black; the holes are the
same cyan rings on all four layers, each with an anti-aliased rim drawn on
every layer whether a trace comes or not.  So the rings are cut out of the
copper (a disk of radius PAD_R round every centre) and a pad joins the
copper areas that reach into the band just outside that disk - a trace
that ends on the ring does, one that only passes by a neighbouring pin
does not.  Areas of different layers meet only in pads (plated through).
The inner planes and layers 3-6 are not in the four images: a pad that
only goes there stays alone.
"""
import json
import os
import sys

import cv2
import numpy as np
from PIL import Image

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
LAYERS = ("NS4-1", "NS4-2", "NS4-7", "NS4-8")
RING = 65            # palette index of the hole rings
OUTLINE = 215        # palette index of the board outline
RING_SIZE = range(18, 28)
PAD_R = 14           # ring with its rim
BAND = 1             # how far past PAD_R a trace end is looked for


def layer_dir():
    if len(sys.argv) > 1:
        return sys.argv[1]
    data = os.environ.get("MS0515_DATA", os.path.join(ROOT, "..", "ms0515_data"))
    return os.path.join(data, "scans", "boards", "radon17-layers")


def find_pads(index):
    """The hole rings: (x, y) centres."""
    rings = (index == RING).astype(np.uint8)
    n, _, stats, centres = cv2.connectedComponentsWithStats(rings, connectivity=8)
    return [(round(float(centres[i, 0]), 1), round(float(centres[i, 1]), 1))
            for i in range(1, n)
            if stats[i, 2] in RING_SIZE and stats[i, 3] in RING_SIZE]


class Union:
    def __init__(self, n):
        self.parent = list(range(n))

    def find(self, a):
        while self.parent[a] != a:
            self.parent[a] = self.parent[self.parent[a]]
            a = self.parent[a]
        return a

    def join(self, a, b):
        self.parent[self.find(a)] = self.find(b)


def band_offsets():
    r = PAD_R + BAND
    ys, xs = np.mgrid[-r:r + 1, -r:r + 1]
    d = np.hypot(xs, ys)
    keep = (d > PAD_R) & (d <= PAD_R + BAND)
    return xs[keep], ys[keep]


def join_layer(index, pads, union):
    copper = ((index != 0) & (index != OUTLINE)).astype(np.uint8)
    for x, y in pads:
        cv2.circle(copper, (round(x), round(y)), PAD_R, 0, thickness=-1)
    _, areas = cv2.connectedComponents(copper, connectivity=8)
    bx, by = band_offsets()
    h, w = areas.shape
    first = {}
    for p, (x, y) in enumerate(pads):
        xs = np.clip(round(x) + bx, 0, w - 1)
        ys = np.clip(round(y) + by, 0, h - 1)
        for area in set(areas[ys, xs].tolist()) - {0}:
            if area in first:
                union.join(p, first[area])
            else:
                first[area] = p


def main():
    folder = layer_dir()
    images = {n: np.array(Image.open(os.path.join(folder, n + ".gif"))) for n in LAYERS}
    pads = find_pads(images["NS4-8"])
    union = Union(len(pads))
    for name in LAYERS:
        join_layer(images[name], pads, union)
    groups = {}
    for p in range(len(pads)):
        groups.setdefault(union.find(p), []).append(p)
    nets = sorted((g for g in groups.values() if len(g) > 1), key=lambda g: g[0])
    out = {"source": "Mick's layer traces of Radon17's scan, zx-pk.ru 15146 post #271",
           "pads": [list(p) for p in pads], "nets": nets}
    with open(os.path.join(ROOT, "netlist", "copper.json"), "w", encoding="utf-8") as f:
        json.dump(out, f, separators=(",", ":"))
    print(f"{len(pads)} pads, {len(nets)} copper nets, "
          f"{sum(len(g) for g in nets)} pads on them")


if __name__ == "__main__":
    main()
