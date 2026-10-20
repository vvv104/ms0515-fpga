#!/usr/bin/env python3
"""Check Mick's layer traces against the bare-board scans of rev. 3А.

    python tools/verify_traces.py

Both scans are warped onto the traces (netlist/registration.json, made by
tools/register.py).  Along the centre line of every drawn run each pixel
of the scan is classed: silver (the outer layer's own tin - layer 1 on
side 1, layer 8 on side 8), gold (the next layer seen through the
laminate - layer 2 under side 1, layer 7 under side 8), or bare.  A drawn
run of an outer layer must be silver; one of an inner layer must be gold
wherever it is not hidden under the outer layer's tin or a ring.  The
pieces of a run where the scan shows nothing are written to
netlist/verify.json as suspects ({layer, x, y, length}) and drawn in
netlist/verify-<layer>.png for looking at.
"""
import json
import os

import cv2
import numpy as np
from PIL import Image

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DATA = os.environ.get("MS0515_DATA", os.path.join(ROOT, "..", "ms0515_data"))
BOARDS = os.path.join(DATA, "scans", "boards")
LAYERS = os.path.join(BOARDS, "radon17-layers")
SIZE = (4488, 3543)
PAD_R = 15
SIDES = {"NS4_red3A_1.tif": ("1", "2"), "NS4_red3A_8.tif": ("8", "7")}
MIN_GAP = 8          # px of a run with nothing under it to report
Image.MAX_IMAGE_PIXELS = None


def warp(name, matrix):
    src = np.array(Image.open(os.path.join(BOARDS, name)).convert("RGB"))
    return cv2.warpAffine(src, np.array(matrix), SIZE,
                          flags=cv2.WARP_INVERSE_MAP | cv2.INTER_AREA)


def classes(img):
    """silver and gold masks of a warped scan."""
    f = img.astype(np.int16)
    r, g, b = f[..., 0], f[..., 1], f[..., 2]
    gray = (r + g + b) / 3
    bg = cv2.medianBlur(gray.astype(np.uint8), 31).astype(np.int16)
    silver = (gray - bg > 35) & (np.abs(r - b) < 40)
    gold = (r - b > 14) & (r > g - 5) & ~silver
    grow = np.ones((3, 3), np.uint8)
    return (cv2.dilate(silver.astype(np.uint8), grow) > 0,
            cv2.dilate(gold.astype(np.uint8), grow) > 0)


def centre_lines(layer, pads):
    idx = np.array(Image.open(os.path.join(LAYERS, f"NS4-{layer}.gif")))
    run = ((idx != 0) & (idx != 215) & (idx != 65)).astype(np.uint8)
    for x, y in pads:
        cv2.circle(run, (round(x), round(y)), PAD_R, 0, thickness=-1)
    return cv2.ximgproc.thinning(run * 255) > 0 if hasattr(cv2, "ximgproc") else \
        cv2.erode(run, np.ones((3, 3), np.uint8)) > 0


def suspects(line, seen, hidden):
    missing = (line & ~seen & ~hidden).astype(np.uint8)
    n, _, stats, centres = cv2.connectedComponentsWithStats(
        cv2.dilate(missing, np.ones((5, 5), np.uint8)), connectivity=8)
    return [(float(centres[i, 0]), float(centres[i, 1]), int(max(stats[i, 2], stats[i, 3])))
            for i in range(1, n) if max(stats[i, 2], stats[i, 3]) >= MIN_GAP]


def main():
    with open(os.path.join(ROOT, "netlist", "registration.json"), encoding="utf-8") as f:
        reg = json.load(f)
    with open(os.path.join(ROOT, "netlist", "copper.json"), encoding="utf-8") as f:
        pads = json.load(f)["pads"]
    rings = np.zeros(SIZE[::-1], np.uint8)
    for x, y in pads:
        cv2.circle(rings, (round(x), round(y)), PAD_R + 3, 1, thickness=-1)
    out = []
    for name, (outer, inner) in SIDES.items():
        img = warp(name, reg[name]["matrix"])
        silver, gold = classes(img)
        for layer, seen, hidden in ((outer, silver, rings > 0),
                                    (inner, gold, (rings > 0) | silver)):
            line = centre_lines(layer, pads)
            found = suspects(line, seen, hidden)
            out += [{"layer": layer, "x": round(x, 1), "y": round(y, 1), "length": n}
                    for x, y, n in found]
            view = img // 3
            view[line] = (0, 160, 255)
            view[line & ~seen & ~hidden] = (255, 0, 0)
            Image.fromarray(view).save(os.path.join(ROOT, "netlist", f"verify-{layer}.png"))
            print(f"layer {layer}: {int(line.sum())} px of runs, "
                  f"{int((line & seen).sum())} seen, {int((line & hidden).sum())} hidden, "
                  f"{len(found)} suspect pieces")
    with open(os.path.join(ROOT, "netlist", "verify.json"), "w", encoding="utf-8") as f:
        json.dump(out, f, indent=0)


if __name__ == "__main__":
    main()
