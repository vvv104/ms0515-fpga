#!/usr/bin/env python3
"""Compare the schematic netlist with the copper of the layer traces.

    python tools/compare.py

Reads netlist/{parts,nets,placement}.csv and netlist/copper.json (made by
tools/copper.py).  Every DIP part of placement.csv is laid on the pads from
its pin 1 (upper right; the upper row runs right to left, the lower row
left to right; 15 mm rows from 24 pins up).  Then:

- **joined**: two pins on one copper net that the schematic puts on two
  different nets - the copper contradicts the drawing (or a net of Э3 was
  misread);
- **apart**: two pins of one schematic net that lie on different copper
  nets - they may meet on the hidden layers 3-6 or through a wire patch, so
  this is a list to look at, not an error.

Power nets are left out: they run on the planes the traces do not show.
Writes the report to netlist/compare.txt.
"""
import csv
import json
import os
from collections import defaultdict

import numpy as np

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
NET = os.path.join(ROOT, "netlist")
PITCH = 39.4
POWER = {"GND", "+5V", "+12V", "-12V"}


def rows(name):
    with open(os.path.join(NET, name), encoding="utf-8") as f:
        return list(csv.DictReader(l for l in f if not l.startswith("#")))


class Pads:
    def __init__(self, pads):
        self.xy = np.array(pads)

    def at(self, x, y):
        d = np.abs(self.xy[:, 0] - x) + np.abs(self.xy[:, 1] - y)
        i = int(np.argmin(d))
        return i if d[i] < 6 else None


def lay(pads, x, y, n):
    """Pad of every pin of an n-pin DIP whose pin 1 is at (x, y)."""
    h, span = n // 2, (236 if n >= 24 else 118)
    top = [pads.at(x - k * PITCH, y) for k in range(h)]
    bottom = [pads.at(x - (h - 1) * PITCH + k * PITCH, y + span) for k in range(h)]
    return top + bottom


def place(pads, parts, placement):
    """{(ref, pin): pad} and the problems of laying the parts down."""
    pin_pad, owner, problems = {}, {}, []
    for r in placement:
        ref = r["ref"]
        laid = lay(pads, float(r["x"]), float(r["y"]), parts[ref])
        for pin, pad in enumerate(laid, 1):
            if pad is None:
                problems.append(f"{ref}.{pin}: no pad")
            elif pad in owner:
                problems.append(f"{ref}.{pin}: pad taken by {owner[pad]}")
            else:
                owner[pad] = f"{ref}.{pin}"
                pin_pad[(ref, pin)] = pad
    return pin_pad, problems


def compare(pin_pad, copper_nets, conns):
    copper_of = {p: i for i, g in enumerate(copper_nets) for p in g}
    net_of = {}
    for c in conns:
        if c["net"] not in POWER:
            net_of[(c["ref"], int(c["pin"]))] = c["net"]
    by_copper = defaultdict(list)
    for key, pad in pin_pad.items():
        if pad in copper_of:
            by_copper[copper_of[pad]].append(key)
    joined = []
    for members in by_copper.values():
        named = sorted({net_of[k] for k in members if k in net_of})
        if len(named) > 1:
            joined.append((named, sorted(f"{r}.{p}" for r, p in members)))
    apart = []
    by_net = defaultdict(set)
    for key, net in net_of.items():
        if key in pin_pad:
            by_net[net].add(copper_of.get(pin_pad[key], ("alone", key)))
    for net, groups in sorted(by_net.items()):
        if len(groups) > 1:
            apart.append((net, len(groups)))
    return joined, apart


def main():
    parts = {r["ref"]: int(r["pins"]) for r in rows("parts.csv")}
    with open(os.path.join(NET, "copper.json"), encoding="utf-8") as f:
        copper = json.load(f)
    pads = Pads(copper["pads"])
    pin_pad, problems = place(pads, parts, rows("placement.csv"))
    joined, apart = compare(pin_pad, copper["nets"], rows("nets.csv"))
    lines = [f"placement: {len(pin_pad)} pins on pads, {len(problems)} problems"]
    lines += ["  " + p for p in problems]
    lines.append(f"joined by copper, apart on the schematic: {len(joined)}")
    lines += [f"  {' | '.join(n)}: {' '.join(m)}" for n, m in joined]
    lines.append(f"one schematic net on several copper nets: {len(apart)}")
    lines += [f"  {n}: {k} pieces" for n, k in apart]
    with open(os.path.join(NET, "compare.txt"), "w", encoding="utf-8") as f:
        f.write("\n".join(lines) + "\n")
    print(lines[0], "|", f"joined {len(joined)}", "|", f"apart {len(apart)}")


if __name__ == "__main__":
    main()
