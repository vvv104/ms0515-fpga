#!/usr/bin/env python3
"""Fit the bare-board scans to the layer traces.

    python tools/register.py

For each scan in scans/boards (MS0515_DATA, as in tools/copper.py) the
holes are found (bright rings round dark centres) and the affine map from
the layer images' pixels to the scan's is fitted to the pads of
netlist/copper.json: a first guess from the scan's turn (side 8 is
photographed turned half round, side 1 flipped top to bottom) and size,
then nearest-hole matching, refined by least squares until it settles.
Writes netlist/registration.json: {scan: {"matrix": 2x3, "matched": n,
"rms": px}}.
"""
import json
import os

import cv2
import numpy as np
from PIL import Image

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DATA = os.environ.get("MS0515_DATA", os.path.join(ROOT, "..", "ms0515_data"))
BOARDS = os.path.join(DATA, "scans", "boards")
LAYER_SIZE = (4488, 3543)
SCANS = {"NS4_red3A_8.tif": "turned", "NS4_red3A_1.tif": "flipped"}
Image.MAX_IMAGE_PIXELS = None


def holes(path, step=2):
    """Hole centres of a scan, in its own pixels."""
    gray = np.array(Image.open(path).convert("L"))[::step, ::step]
    gray = cv2.GaussianBlur(gray, (5, 5), 0)
    circles = cv2.HoughCircles(gray, cv2.HOUGH_GRADIENT, dp=1, minDist=8,
                               param1=80, param2=14, minRadius=4, maxRadius=12)
    return np.zeros((0, 2)) if circles is None else circles[0, :, :2] * step


def guess(kind, size):
    w, h = LAYER_SIZE
    k = size[0] / w
    if kind == "turned":
        return np.array([[-k, 0, k * w], [0, -k, k * h]])
    return np.array([[k, 0, 0], [0, -k, k * h]])


def dots(points, shape, cell):
    img = np.zeros(shape, np.float32)
    ij = (points / cell).astype(int)
    ok = (ij[:, 0] >= 0) & (ij[:, 0] < shape[1]) & (ij[:, 1] >= 0) & (ij[:, 1] < shape[0])
    img[ij[ok, 1], ij[ok, 0]] = 1
    return cv2.GaussianBlur(img, (5, 5), 1.2)


def shift(m, pads, found, size, cell=8):
    """Scale and offset of the guess by phase correlation of the dot maps."""
    shape = (size[1] // cell + 1, size[0] // cell + 1)
    target = dots(found, shape, cell)
    window = cv2.createHanningWindow((shape[1], shape[0]), cv2.CV_32F)
    best = None
    for s in np.arange(0.90, 1.10, 0.004):
        trial = m.copy()
        trial[:, :2] *= s
        trial[:, 2] = m[:, 2] * s + (1 - s) * np.array(size) / 2
        img = dots(np.c_[pads, np.ones(len(pads))] @ trial.T, shape, cell)
        (dx, dy), peak = cv2.phaseCorrelate(img, target, window)
        if best is None or peak > best[0]:
            best = (peak, trial, dx * cell, dy * cell)
    _, m, dx, dy = best
    m = m.copy()
    m[:, 2] += [dx, dy]
    return m


def refine(m, pads, found, size, rounds=8):
    m = shift(m, pads, found, size)
    tree = cv2.flann_Index(found.astype(np.float32), {"algorithm": 1, "trees": 4})
    tol = 12.0
    for _ in range(rounds):
        mapped = (np.c_[pads, np.ones(len(pads))] @ m.T).astype(np.float32)
        idx, dist = tree.knnSearch(mapped, 1, params={})
        near = np.sqrt(dist[:, 0]) < tol
        src = pads[near]
        dst = found[idx[near, 0]]
        a, *_ = np.linalg.lstsq(np.c_[src, np.ones(len(src))], dst, rcond=None)
        m = a.T
        err = np.sqrt(((np.c_[src, np.ones(len(src))] @ m.T - dst) ** 2).sum(1))
        tol = max(4.0, min(12.0, float(np.percentile(err, 90)) * 2))
    return m, int(near.sum()), float(np.sqrt((err ** 2).mean()))


def main():
    with open(os.path.join(ROOT, "netlist", "copper.json"), encoding="utf-8") as f:
        pads = np.array(json.load(f)["pads"])
    out = {}
    for name, kind in SCANS.items():
        path = os.path.join(BOARDS, name)
        size = Image.open(path).size
        found = holes(path)
        m, matched, rms = refine(guess(kind, size), pads, found, size)
        out[name] = {"matrix": m.round(6).tolist(), "matched": matched, "rms": round(rms, 2)}
        print(f"{name}: {len(found)} holes, {matched}/{len(pads)} pads matched, rms {rms:.2f} px")
    with open(os.path.join(ROOT, "netlist", "registration.json"), "w", encoding="utf-8") as f:
        json.dump(out, f, indent=1)


if __name__ == "__main__":
    main()
