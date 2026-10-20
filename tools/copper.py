#!/usr/bin/env python3
"""Read the copper of НС4 out of Radon17's layer traces.

    python tools/copper.py [LAYER_DIR]

LAYER_DIR  the folder with NS4-1.gif, NS4-2.gif, NS4-7.gif, NS4-8.gif
           (default: $MS0515_DATA/scans/boards/radon17-layers, MS0515_DATA
           falling back to ../ms0515_data beside the repository)

Writes netlist/copper.json: every plated hole ("pad", its centre in the
layer images' pixels, 4488x3543, layer 8 = the component side as seen from
it) and the groups of pads one copper area joins on any of the four layers.

How the images are read: each layer is drawn on black; the holes are the
same cyan rings on all four layers, the copper of a layer is everything
else that is not black, the white board outline excepted.  A pad belongs to
the copper area its ring touches; areas of different layers meet only in
pads (the holes are plated through).  The inner planes the four images do
not show (power and ground) are not seen here: a pad that only goes to a
plane stays alone.
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


def layer_dir():
    if len(sys.argv) > 1:
        return sys.argv[1]
    data = os.environ.get("MS0515_DATA", os.path.join(ROOT, "..", "ms0515_data"))
    return os.path.join(data, "scans", "boards", "radon17-layers")


def find_pads(index):
    """The hole rings: (x, y) centres and a label image of the rings."""
    rings = (index == RING).astype(np.uint8)
    n, labels, stats, centres = cv2.connectedComponentsWithStats(rings, connectivity=8)
    pads, keep = [], np.zeros(n, dtype=np.int32) - 1
    for i in range(1, n):
        w, h = stats[i, 2], stats[i, 3]
        if w in RING_SIZE and h in RING_SIZE:
            keep[i] = len(pads)
            pads.append((round(float(centres[i, 0]), 1), round(float(centres[i, 1]), 1)))
    return pads, keep[labels]


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


def join_layer(index, pad_of_pixel, union):
    copper = ((index != 0) & (index != OUTLINE)).astype(np.uint8)
    _, areas = cv2.connectedComponents(copper, connectivity=8)
    on_pad = pad_of_pixel >= 0
    first = {}
    for area, pad in zip(areas[on_pad].tolist(), pad_of_pixel[on_pad].tolist()):
        if area in first:
            union.join(pad, first[area])
        else:
            first[area] = pad


def main():
    folder = layer_dir()
    images = {n: np.array(Image.open(os.path.join(folder, n + ".gif"))) for n in LAYERS}
    pads, pad_of_pixel = find_pads(images["NS4-8"])
    union = Union(len(pads))
    for name in LAYERS:
        join_layer(images[name], pad_of_pixel, union)
    groups = {}
    for p in range(len(pads)):
        groups.setdefault(union.find(p), []).append(p)
    nets = sorted((g for g in groups.values() if len(g) > 1), key=lambda g: g[0])
    out = {"source": "Radon17's layer traces, zx-pk.ru 15146 post #271",
           "pads": [list(p) for p in pads], "nets": nets}
    with open(os.path.join(ROOT, "netlist", "copper.json"), "w", encoding="utf-8") as f:
        json.dump(out, f, separators=(",", ":"))
    print(f"{len(pads)} pads, {len(nets)} copper nets, "
          f"{sum(len(g) for g in nets)} pads on them")


if __name__ == "__main__":
    main()
