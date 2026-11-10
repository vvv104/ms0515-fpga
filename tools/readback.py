#!/usr/bin/env python3
"""The routed board's netlist read off its copper, against netlist/ (docs/plan.md 8e).

    <KiCad>/bin/python tools/readback.py [BOARD] [variant]   - exit 1 on a difference

The net names on the board's copper are ours, so they are not used: the pads,
tracks, vias and the planes' fill are joined where their copper touches, each
group of pins so found is a net of the board as it would be made, and these
groups must be the netlist's nets - every net's pins in one group, no group
with the pins of two (tools/pcb.py's pin_nets: Э3 or a variant, the chips in
sockets on the sockets' pins).  A pad is its copper's circle or box; a plane
reaches a pad where its fill overlaps the pad's copper (a thermal spoke).
"""
import math
import os
import sys
from collections import defaultdict

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
CELL = 5.0


def _seg_point(px, py, t):
    dx, dy = t["x1"] - t["x0"], t["y1"] - t["y0"]
    k = ((px - t["x0"]) * dx + (py - t["y0"]) * dy) / (dx * dx + dy * dy or 1.0)
    k = min(1.0, max(0.0, k))
    return math.hypot(t["x0"] + k * dx - px, t["y0"] + k * dy - py)


def _seg_seg(a, b):
    """Distance between two segments (they cross: 0)."""
    def side(p, q, r):
        return (q[0] - p[0]) * (r[1] - p[1]) - (q[1] - p[1]) * (r[0] - p[0])
    a0, a1, b0, b1 = (a["x0"], a["y0"]), (a["x1"], a["y1"]), (b["x0"], b["y0"]), (b["x1"], b["y1"])
    if side(a0, a1, b0) * side(a0, a1, b1) < 0 and side(b0, b1, a0) * side(b0, b1, a1) < 0:
        return 0.0
    return min(_seg_point(*b0, a), _seg_point(*b1, a), _seg_point(*a0, b), _seg_point(*a1, b))


def _pad_reach(p, t):
    """From a track's centre line to a pad's copper (a circle, or a box half w x half h)."""
    if "hw" not in p:
        return _seg_point(p["x"], p["y"], t) - p["r"]
    best = math.inf
    for k in range(33):
        x = t["x0"] + (t["x1"] - t["x0"]) * k / 32
        y = t["y0"] + (t["y1"] - t["y0"]) * k / 32
        best = min(best, math.hypot(max(abs(x - p["x"]) - p["hw"], 0.0), max(abs(y - p["y"]) - p["hh"], 0.0)))
    return best


def _cells(x0, y0, x1, y1, grow):
    return [(gx, gy) for gx in range(int((min(x0, x1) - grow) // CELL), int((max(x0, x1) + grow) // CELL) + 1)
            for gy in range(int((min(y0, y1) - grow) // CELL), int((max(y0, y1) + grow) // CELL) + 1)]


def pin_groups(pads, tracks, vias, planes):
    """[{pin}]: the pads whose copper joins, through the tracks (a layer each), the
    vias and the pads (every layer) and the planes ({"pins": the pads the fill reaches})."""
    items = [("pad", p) for p in pads] + [("via", v) for v in vias] + [("track", t) for t in tracks]
    parent = list(range(len(items)))

    def find(a):
        while parent[a] != a:
            parent[a] = parent[parent[a]]
            a = parent[a]
        return a

    def join(a, b):
        parent[find(a)] = find(b)
    grid = defaultdict(list)
    for k, (kind, it) in enumerate(items):
        if kind == "track":
            box = (it["x0"], it["y0"], it["x1"], it["y1"], it["w"])
        else:
            box = (it["x"], it["y"], it["x"], it["y"], max(it["r"], it.get("hw", 0), it.get("hh", 0)))
        for c in _cells(*box[:4], box[4]):
            grid[c].append(k)
    for k, (kind, t) in enumerate(items):
        if kind != "track":
            continue
        near = {j for c in _cells(t["x0"], t["y0"], t["x1"], t["y1"], t["w"]) for j in grid[c] if j != k}
        for j in near:
            okind, o = items[j]
            if okind == "track":
                touch = o["layer"] == t["layer"] and _seg_seg(t, o) <= (t["w"] + o["w"]) / 2
            elif okind == "via":
                touch = _seg_point(o["x"], o["y"], t) <= o["r"] + t["w"] / 2
            else:
                touch = _pad_reach(o, t) <= t["w"] / 2
            if touch:
                join(k, j)
    index = {p["pin"]: k for k, p in enumerate(pads)}
    for plane in planes:
        pins = [index[p] for p in plane["pins"] if p in index]
        for a in pins[1:]:
            join(a, pins[0])
    for k, (kind, v) in enumerate(items):           # a via standing in a pad's copper
        if kind == "via":
            for j in {j for c in _cells(v["x"], v["y"], v["x"], v["y"], v["r"]) for j in grid[c]}:
                if items[j][0] == "pad" and math.hypot(items[j][1]["x"] - v["x"], items[j][1]["y"] - v["y"]) \
                        < items[j][1]["r"] + v["r"]:
                    join(k, j)
    groups = defaultdict(set)
    for k, p in enumerate(pads):
        groups[find(k)].add(p["pin"])
    return sorted(groups.values(), key=lambda g: sorted(g))


PLANES = ("GND", "+5V")


def plane_splits(groups, want):
    """{net: [[pin]]}: a plane's net in more than one group - the pins of every group but
    the largest (pieces of the fill cut off the plane), for tools/pathfind.py to join."""
    out = {}
    for net in PLANES:
        parts = sorted((sorted(p for p in g if want.get(p) == net) for g in groups
                        if any(want.get(p) == net for p in g)), key=len, reverse=True)
        if len(parts) > 1:
            out[net] = parts[1:]
    return out


def compare(groups, want):
    """The differences between the board's groups of pins and the netlist ({pin: net})."""
    out = []
    pieces = defaultdict(list)
    for g in groups:
        nets = sorted({want.get(p, "(no net)") for p in g}, key=lambda n: (n == "(no net)", n))
        for n in nets:
            if n != "(no net)":
                pieces[n].append(sorted(p for p in g if want.get(p) == n))
        real = [n for n in nets if n != "(no net)"]
        if len(nets) > 1 and real:
            out.append(f"{', '.join(nets)} joined: {', '.join(sorted(g))}")
    split = [f"{n}: in {len(ps)} pieces: " + " | ".join(", ".join(p) for p in sorted(ps))
             for n, ps in pieces.items() if len(ps) > 1]
    return sorted(split) + out


STEP = 0.05                     # mm a pixel of a plane's fill


def pieces(labels, pads, origin, step):
    """[{"pins"}]: per piece of fill (labels: 0 none, k the k-th piece) the pads it overlaps."""
    import numpy as np
    h, w = labels.shape
    on = defaultdict(set)
    for p in pads:
        cx, cy, r = (p["x"] - origin[0]) / step, (p["y"] - origin[1]) / step, p["r"] / step
        ys, xs = np.mgrid[int(cy - r):int(cy + r) + 2, int(cx - r):int(cx + r) + 2]
        inside = np.hypot(xs - cx, ys - cy) < r * 0.95
        for k in set(labels[ys.clip(0, h - 1), xs.clip(0, w - 1)][inside].tolist()) - {0}:
            on[k].add(p["pin"])
    return [{"pins": on[k]} for k in sorted(on)]


def plane_pins(board, pads):
    """[{"pins"}]: per piece of each zone's fill (an outline of it: the fill may fall in
    islands) the pads it overlaps, rasterised at STEP."""
    import numpy as np
    import pcbnew
    from PIL import Image, ImageDraw
    mm = pcbnew.ToMM
    box = board.GetBoardEdgesBoundingBox()
    x0, y0 = mm(box.GetLeft()), mm(box.GetTop())
    w, h = int(mm(box.GetWidth()) / STEP) + 2, int(mm(box.GetHeight()) / STEP) + 2

    def px(p):
        return ((mm(p.x) - x0) / STEP, (mm(p.y) - y0) / STEP)
    out = []
    for zone in board.Zones():
        for layer in zone.GetLayerSet().Seq():
            polys = zone.GetFilledPolysList(layer)
            img = Image.new("I", (w, h), 0)
            draw = ImageDraw.Draw(img)
            # the largest first: an island stands in a hole of a larger piece, drawn after it
            order = sorted(range(polys.OutlineCount()), key=lambda k: -abs(polys.Outline(k).Area()))
            for label, k in enumerate(order, 1):
                o = polys.Outline(k)
                draw.polygon([px(o.CPoint(i)) for i in range(o.PointCount())], fill=label)
                for q in range(polys.HoleCount(k)):
                    hole = polys.Hole(k, q)
                    draw.polygon([px(hole.CPoint(i)) for i in range(hole.PointCount())], fill=0)
            out += pieces(np.array(img), pads, (x0, y0), STEP)
    return out


def read_board(path):
    """(pads, tracks, vias, planes) of the board, in mm, net names left out."""
    import pcbnew
    board = pcbnew.LoadBoard(path)
    pcbnew.ZONE_FILLER(board).Fill(board.Zones())
    mm = pcbnew.ToMM
    pads, tracks, vias = [], [], []
    for p in board.GetPads():
        if not p.GetNumber():
            continue
        s = p.GetSize(pcbnew.F_Cu)
        item = {"pin": f"{p.GetParentFootprint().GetReference()}.{p.GetNumber()}",
                "x": mm(p.GetPosition().x), "y": mm(p.GetPosition().y), "r": mm(min(s.x, s.y)) / 2}
        if p.GetShape(pcbnew.F_Cu) not in (pcbnew.PAD_SHAPE_CIRCLE, pcbnew.PAD_SHAPE_OVAL):
            hw, hh = mm(s.x) / 2, mm(s.y) / 2
            item["hw"], item["hh"] = (hh, hw) if round(p.GetOrientation().AsDegrees()) % 180 == 90 else (hw, hh)
        pads.append(item)
    for t in board.GetTracks():
        if t.GetClass() == "PCB_VIA":
            vias.append({"x": mm(t.GetPosition().x), "y": mm(t.GetPosition().y), "r": mm(t.GetWidth(pcbnew.F_Cu)) / 2})
        else:
            tracks.append({"layer": board.GetLayerName(t.GetLayer()), "x0": mm(t.GetStart().x), "y0": mm(t.GetStart().y),
                           "x1": mm(t.GetEnd().x), "y1": mm(t.GetEnd().y), "w": mm(t.GetWidth())})
    return pads, tracks, vias, plane_pins(board, pads)


def wanted(variant):
    """{pin: net} of the netlist, as tools/pcb.py's check reads it."""
    sys.path.insert(0, HERE)
    import pcb
    kinds = {ref: item.type for ref, item in pcb.load_items().items()}
    return {f"{ref}.{pcb.pad_of(kinds[ref], pin)}": net
            for (ref, pin), net in pcb.pin_nets(variant).items() if ref in kinds}


def main(argv):
    path = argv[1] if len(argv) > 1 else os.path.join(ROOT, "build", "pathfind", "ns4-pathfind.kicad_pcb")
    variant = argv[2] if len(argv) > 2 else None
    pads, tracks, vias, planes = read_board(path)
    groups = pin_groups(pads, tracks, vias, planes)
    diffs = compare(groups, wanted(variant))
    for d in diffs:
        print(d)
    print(f"{len(pads)} pins, {len(tracks)} track pieces, {len(vias)} vias, {len(planes)} planes: "
          f"{len(groups)} groups of copper; {len(diffs)} differences from the netlist")
    return 1 if diffs else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
