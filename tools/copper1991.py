#!/usr/bin/env python3
"""Mick's layer traces of НС4 as centre lines (docs/plan.md stage 8d).

    python tools/copper1991.py [LAYER_DIR]

LAYER_DIR  the folder with NS4-1.gif, NS4-2.gif, NS4-7.gif, NS4-8.gif
           (default: $MS0515_DATA/scans/boards/radon17-layers, MS0515_DATA
           falling back to ../ms0515_data beside the repository)

Writes netlist/lines-1991.json: for each of the four layers, its copper as
centre lines - {"w": width px, "pts": [[x, y], ...], "holes": [hole, ...]} -
in the layer images' pixels, the holes those of netlist/copper.json that the
line's copper area reaches (copper.py's rule: the hole rings cut out, an area
reaching into the band just outside a ring is on that hole).  tools/lay1991.py
gives the lines nets; tools/pcb.py lays them as the board's outer layers.

How: each layer's copper (any colour but the black, the board outline and the
rings) with a disk cut out round every hole, opened by 2 px (the drawing's
anti-aliased crumbs), thinned to a one-pixel skeleton (Zhang-Suen, OpenCV's
ximgproc), and the skeleton traced into polylines from every end or junction
to the next, simplified to 1 px; a line's width is twice the median distance
from its pixels to the copper's edge.
"""
import json
import os
import sys
from collections import defaultdict

import cv2
import numpy as np
from PIL import Image

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
LAYERS = ("NS4-1", "NS4-2", "NS4-7", "NS4-8")
RING, OUTLINE = 65, 215              # palette indices, as tools/copper.py reads them
PAD_R, BAND = 14, 1
N8 = [(-1, -1), (0, -1), (1, -1), (-1, 0), (1, 0), (-1, 1), (0, 1), (1, 1)]


def layer_dir():
    if len(sys.argv) > 1:
        return sys.argv[1]
    data = os.environ.get("MS0515_DATA", os.path.join(ROOT, "..", "ms0515_data"))
    return os.path.join(data, "scans", "boards", "radon17-layers")


def junctions(on, step):
    """{pixel: centre} for the skeleton's junction pixels: a clump of
    touching junction pixels is one junction, at the clump's centre."""
    joints = {p for p in on if len(step(p)) > 2}
    centre = {}
    for p in sorted(joints):
        if p in centre:
            continue
        clump, todo = [], [p]
        centre[p] = None
        while todo:
            q = todo.pop()
            clump.append(q)
            for r in step(q):
                if r in joints and r not in centre:
                    centre[r] = None
                    todo.append(r)
        c = [round(sum(v[0] for v in clump) / len(clump)), round(sum(v[1] for v in clump) / len(clump))]
        centre.update({q: c for q in clump})
    return centre


def trace(skel):
    """A skeleton's polylines: from each end or junction along the
    unwalked pixels to the next end or junction (or round a loop); a line
    ending in a junction ends at the junction's centre, and the pieces
    inside one junction are dropped."""
    ys, xs = np.nonzero(skel)
    on = set(zip(xs.tolist(), ys.tolist()))
    step = lambda p: [(p[0] + a, p[1] + b) for a, b in N8 if (p[0] + a, p[1] + b) in on]
    nodes = {p for p in on if len(step(p)) != 2}
    centre = junctions(on, step)
    lines = []
    for path in _walk(on, step, nodes):
        if all(p in centre for p in path):
            continue
        pts = [list(p) for p in path]
        for k in (0, -1):
            if path[k] in centre:
                pts[k] = centre[path[k]]
        lines.append(pts)
    return lines


def _walk(on, step, nodes):
    walked, lines = set(), []
    for start in sorted(nodes) + sorted(on - nodes):
        for nxt in step(start):
            if (start, nxt) in walked:
                continue
            path, prev, cur = [start], start, nxt
            walked.add((start, nxt))
            while True:
                path.append(cur)
                walked.add((cur, prev))
                if cur in nodes or cur == start:
                    break
                ahead = [q for q in step(cur) if q != prev and (cur, q) not in walked]
                if not ahead:
                    break
                ahead.sort(key=lambda q: abs(q[0] - cur[0]) + abs(q[1] - cur[1]))
                walked.add((cur, ahead[0]))
                prev, cur = cur, ahead[0]
            lines.append(path)
    return lines


def area_holes(areas, holes):
    """{area: {hole}}: the holes whose band each copper area reaches."""
    ys, xs = np.mgrid[-PAD_R - BAND:PAD_R + BAND + 1, -PAD_R - BAND:PAD_R + BAND + 1]
    d = np.hypot(xs, ys)
    keep = (d > PAD_R) & (d <= PAD_R + BAND)
    bx, by = xs[keep], ys[keep]
    h, w = areas.shape
    out = defaultdict(set)
    for i, (x, y) in enumerate(holes):
        hit = areas[np.clip(round(y) + by, 0, h - 1), np.clip(round(x) + bx, 0, w - 1)]
        for a in set(hit.tolist()) - {0}:
            out[a].add(i)
    return out


def layer_lines(index, holes):
    """One layer image's (palette indices) centre lines."""
    copper = ((index != 0) & (index != OUTLINE) & (index != RING)).astype(np.uint8)
    for x, y in holes:
        cv2.circle(copper, (round(x), round(y)), PAD_R, 0, thickness=-1)
    copper = cv2.morphologyEx(copper, cv2.MORPH_OPEN, np.ones((2, 2), np.uint8))
    _, areas = cv2.connectedComponents(copper, connectivity=8)
    reach = area_holes(areas, holes)
    dist = cv2.distanceTransform(copper, cv2.DIST_L2, 5)
    skel = cv2.ximgproc.thinning(copper * 255) > 0
    out = []
    for path in trace(skel):
        poly = cv2.approxPolyDP(np.array(path, np.int32).reshape(-1, 1, 2), 1.0, False).reshape(-1, 2)
        width = 2 * float(np.median([dist[y, x] for x, y in path])) - 1
        mid = path[len(path) // 2]
        a = int(areas[mid[1], mid[0]])
        out.append({"w": round(width, 1), "pts": poly.tolist(), "holes": sorted(reach.get(a, ()))})
    return out


def main():
    folder = layer_dir()
    with open(os.path.join(ROOT, "netlist", "copper.json"), encoding="utf-8") as f:
        holes = json.load(f)["pads"]
    result = {"source": "Mick's layer traces of Radon17's scan, zx-pk.ru 15146 post #271; tools/copper1991.py"}
    for name in LAYERS:
        index = np.array(Image.open(os.path.join(folder, name + ".gif")))
        result[name] = layer_lines(index, holes)
        print(f"{name}: {len(result[name])} lines")
    with open(os.path.join(ROOT, "netlist", "lines-1991.json"), "w", encoding="utf-8", newline="\n") as f:
        json.dump(result, f, separators=(",", ":"))
    return 0


if __name__ == "__main__":
    sys.exit(main())
