#!/usr/bin/env python3
"""What the 1991 board's hidden layers 3..6 must join, as a list to ring out.

    <KiCad>/bin/python tools/inner1991.py      - write docs/pcb/inner-layers.md

Mick's traces show the board's layers 1, 2, 7 and 8 (netlist/lines-1991.json,
tools/copper1991.py).  The holes their copper joins fall into groups; a net of
Э3 (the pins of kicad/ns4.kicad_pcb, Э3's board) whose pins lie in more than
one group is joined inside, on layers 3..6 that no scan shows.  For each such
net the list names one pin of each group and the pin each is to ring to - a
continuity check on a real board, from outside, with nothing taken apart.
And where the outer copper joins pins of two nets of Э3 (netlist/compare.txt's
"joined by copper"), the list names a pin of each: those must not ring.
"""
import json
import os
import sys
from collections import defaultdict

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
OUT = os.path.join(ROOT, "docs", "pcb", "inner-layers.md")
PLANES = ("GND", "+5V")


def outer_groups(lines, n_holes):
    """{hole: group} of the holes the outer copper joins (any layer)."""
    parent = list(range(n_holes))

    def find(a):
        while parent[a] != a:
            parent[a] = parent[parent[a]]
            a = parent[a]
        return a
    for layer in ("NS4-1", "NS4-2", "NS4-7", "NS4-8"):
        for ln in lines[layer]:
            for h in ln["holes"][1:]:
                parent[find(h)] = find(ln["holes"][0])
    return {h: find(h) for h in range(n_holes)}


def pin_name(ref, pad):
    return f"{ref}.{pad}"


def inner_joins(pins, group):
    """{net: [(pin, to pin)]}: per net, one pin of each outer group past the
    first and the first group's pin it must ring to.  pins: [(net, name, hole)]."""
    by_net = defaultdict(lambda: defaultdict(list))
    for net, name, hole in pins:
        by_net[net][group[hole]].append(name)
    out = {}
    for net, groups in by_net.items():
        if len(groups) < 2:
            continue
        heads = sorted(sorted(g, key=_natural)[0] for g in groups.values())
        heads.sort(key=_natural)
        out[net] = [(h, heads[0]) for h in heads[1:]]
    return out


def outer_shorts(pins, group):
    """[(net a, pin, net b, pin)]: outer groups holding pins of two nets."""
    nets = defaultdict(lambda: defaultdict(list))
    for net, name, hole in pins:
        nets[group[hole]][net].append(name)
    out = []
    for g, members in nets.items():
        if len(members) > 1:
            names = sorted(members, key=lambda n: -len(members[n]))
            first = sorted(members[names[0]], key=_natural)[0]
            for other in names[1:]:
                out.append((names[0], first, other, sorted(members[other], key=_natural)[0]))
    return sorted(out)


def _natural(name):
    ref, _, pad = name.partition(".")
    head = ref.rstrip("0123456789")
    return head, int(ref[len(head):] or 0), pad.zfill(3)


def board_pins():
    """[(net, pin name, hole)] of Э3's board's pads on the 1991 holes."""
    import pcbnew
    sys.path.insert(0, HERE)
    import pcb
    import netlist as nl
    board = pcbnew.LoadBoard(os.path.join(ROOT, "kicad", "ns4.kicad_pcb"))
    holes = json.load(open(os.path.join(ROOT, "netlist", "copper.json"), encoding="utf-8"))["pads"]
    nets = pcb.hole_nets(board, holes)
    names = {}
    for fp in board.GetFootprints():
        for pad in fp.Pads():
            x, y = (pcbnew.ToMM(v) / pcb.MM_PER_PX for v in (pad.GetPosition().x, pad.GetPosition().y))
            for h, net in nets.items():
                if abs(holes[h][0] - x) < 5 and abs(holes[h][1] - y) < 5:
                    names[h] = pin_name(fp.GetReference(), pad.GetNumber())
    own = {r["ref"] for r in nl.rows(os.path.join(ROOT, "netlist", "pcb_parts.csv"))}
    back = {f"{r['ref']}.{r['pin']}": r["from_net"] for r in nl.rows(os.path.join(ROOT, "netlist", "pcb.csv"))}
    pins = []
    for h, net in nets.items():
        name = names.get(h)
        if name is None or name.split(".")[0] in own:      # the board's own parts: not the 1991 board's
            continue
        pins.append((back.get(name, net), name, h))          # a pin the board moved: on Э3's net
    return pins, len(holes)


def write(joins, shorts, planes):
    text = ["# What the hidden layers join: a list to ring out", "",
            "Written by `tools/inner1991.py`; how and why: its docstring and `docs/plan.md` 8d.", "",
            "НС4 rev. 3А has eight copper layers; Mick's traces (zx-pk.ru 15146 #271) show 1, 2, 7 and 8.",
            "A net of Э3 whose pins the outer copper leaves in pieces is joined on layers 3..6.  With a",
            "board and an ohmmeter (power off), each line below is a pair that must ring: the first pin",
            "of a piece and the pin of the first piece.  Copper rings at well under an ohm; through the",
            "chips' inputs and outputs two pins read hundreds of ohms or more, which is not a ring.  A",
            "pair that does not ring is a place where the board differs from Э3 - or where our reading",
            "of Mick's traces does; either way it is worth a photo and a post.", "",
            f"`GND` and `+5V` are planes: every pin of theirs rings to every other ({planes}).", ""]
    text += ["## Pairs that must ring", "", "| Net | Pin | Rings to |", "|---|---|---|"]
    for net in sorted(joins):
        for a, b in joins[net]:
            text.append(f"| `{net}` | {a} | {b} |")
    text += ["", "## Pairs that must not ring", "",
             "The outer copper as traced joins these pins of two nets of Э3: a trace error, a factory",
             "patch (a cut and a wire), or the board's own.  On a board built to Э3 they are apart.", "",
             "| Net | Pin | Net | Pin |", "|---|---|---|---|"]
    for a, pa, b, pb in shorts:
        text.append(f"| `{a}` | {pa} | `{b}` | {pb} |")
    with open(OUT, "w", encoding="utf-8", newline="\n") as f:
        f.write("\n".join(text) + "\n")


def main():
    pins, n = board_pins()
    lines = json.load(open(os.path.join(ROOT, "netlist", "lines-1991.json"), encoding="utf-8"))
    group = outer_groups(lines, n)
    signal = [p for p in pins if p[0] not in PLANES]
    planes = ", ".join(f"{sum(1 for p in pins if p[0] == pl)} `{pl}` pins" for pl in PLANES)
    joins, shorts = inner_joins(signal, group), outer_shorts(pins, group)
    write(joins, shorts, planes)
    print(f"{len(joins)} nets joined inside, {sum(len(v) for v in joins.values())} pairs; "
          f"{len(shorts)} outer joins of two nets -> {OUT}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
