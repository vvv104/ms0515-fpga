#!/usr/bin/env python3
"""НС4 as a printed board made anew (docs/plan.md stage 8d), for KiCad 10.

Run with KiCad's own Python, which has pcbnew:

    <KiCad>/bin/python tools/pcb.py          - write kicad/ns4.kicad_pcb
    <KiCad>/bin/python tools/pcb.py check    - the board's pads against the
                                               netlist (stage 8e), exit 1 on
                                               a difference

The board is the netlist's (netlist/parts.csv, nets.csv - Э3 as read, the
one the board model runs the machine's software on) with the footprints of
tools/bom.py.  Four layers: signals on the outer ones, ground on In1.Cu
and +5 V on In2.Cu as planes.  The chips and the resistor packs stand
where they stood on the 1991 board (netlist/placement.csv: pin 1 of every
DIP on Mick's layer traces, 2.54 mm a 39.4 pixels), which the plant
routed; the connectors go along the left edge, where that board had
them; every other part goes into the free place nearest the chip it
works for.  Routing is Freerouting's (tools/route.py).
"""
import math
import os
import sys
from collections import defaultdict

import pcbnew

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import bom  # noqa: E402
import kicad  # noqa: E402
import netlist as nl  # noqa: E402

ROOT = os.path.dirname(HERE)
NET = os.path.join(ROOT, "netlist")
OUT = os.path.join(ROOT, "kicad", "ns4.kicad_pcb")
MM_PER_PX = 2.54 / 39.4
BOARD_W, BOARD_H = 290.0, 230.0          # mm, about the 1991 board's
DIP_ROTATION = 270                        # pin 1 upper right, the upper row right to left
DIODES = ("КД522Б", "КС133А", "АЛ307БМ")  # the netlist's pin 1 is the anode, KiCad's pad 1 the cathode
CLEARANCE = 1.0                           # mm between courtyards when parts are placed


def footprint_dirs():
    """{library: folder}: KiCad's own (beside its Python) and kicad/ns4.pretty."""
    share = os.environ.get("KICAD10_FOOTPRINT_DIR") or os.path.normpath(
        os.path.join(os.path.dirname(pcbnew.__file__), "..", "..", "..", "share", "kicad", "footprints"))
    dirs = {name[:-len(".pretty")]: os.path.join(share, name)
            for name in os.listdir(share) if name.endswith(".pretty")}
    dirs["ns4"] = os.path.join(ROOT, "kicad", "ns4.pretty")
    return dirs


def pad_of(kind, pin):
    """The footprint's pad for a pin of the netlist."""
    if kind in DIODES:
        return {1: "2", 2: "1"}[pin]
    if kind == "СНП-59-96":                 # rows A, B, C of 32
        return "abc"[(pin - 1) // 32] + str((pin - 1) % 32 + 1)
    return str(pin)


def pin_nets():
    """{(ref, pin): net} with the chips' power pins; a chip in a socket's
    pins given to the socket."""
    parts, _, conns = nl.load(os.path.join(NET, "parts.csv"), os.path.join(NET, "pinouts.csv"),
                              os.path.join(NET, "nets.csv"))
    nets = {}
    for net, pins in kicad.nets_of(parts, conns).items():
        for ref, pin in pins:
            nets[(bom.SOCKETS.get(ref, ref), int(pin))] = net
    return nets


def load_items():
    found, problems = bom.items(os.path.join(NET, "parts.csv"), os.path.join(NET, "packages.csv"),
                                os.path.join(ROOT, "docs", "chips.md"), os.path.join(NET, "firmware.csv"))
    if problems:
        raise SystemExit("\n".join(problems))
    return {ref: item for ref, item in found.items() if item.placed}


def new_footprint(board, dirs, item):
    lib, name = item.footprint.split(":")
    if lib not in dirs or not os.path.exists(os.path.join(dirs[lib], name + ".kicad_mod")):
        return None
    fp = pcbnew.FootprintLoad(dirs[lib], name)
    fp.SetReference(item.ref)
    fp.SetValue(item.value or item.type)
    board.Add(fp)
    return fp


def connect(board, fp, kind, nets, codes):
    """Each pad its net; returns the pins of the netlist the footprint has
    no pad for."""
    pads = {pad.GetNumber(): pad for pad in fp.Pads()}
    missing = []
    for (ref, pin), net in nets.items():
        if ref != fp.GetReference():
            continue
        pad = pads.get(pad_of(kind, pin))
        if pad is None:
            missing.append(f"{ref}.{pin}")
            continue
        if net not in codes:
            item = pcbnew.NETINFO_ITEM(board, net)
            board.Add(item)
            codes[net] = item
        pad.SetNet(codes[net])
    return missing


def originals():
    """{ref: (x, y)} in mm: pin 1 of every DIP of the 1991 board."""
    return {r["ref"]: (float(r["x"]) * MM_PER_PX, float(r["y"]) * MM_PER_PX)
            for r in nl.rows(os.path.join(NET, "placement.csv"))}


class Free:
    """The board's place, a grid of 0.5 mm cells taken by courtyards."""

    STEP = 0.5

    def __init__(self, board):
        self.w, self.h = int(BOARD_W / self.STEP), int(BOARD_H / self.STEP)
        self.taken = bytearray(self.w * self.h)

    def box(self, fp):
        b = fp.GetBoundingBox(False)
        m = CLEARANCE / 2
        return (pcbnew.ToMM(b.GetLeft()) - m, pcbnew.ToMM(b.GetTop()) - m,
                pcbnew.ToMM(b.GetRight()) + m, pcbnew.ToMM(b.GetBottom()) + m)

    def cells(self, box):
        x0, y0, x1, y1 = (int(math.floor(v / self.STEP)) for v in box)
        return x0, y0, x1, y1

    def fits(self, box):
        x0, y0, x1, y1 = self.cells(box)
        if x0 < 4 or y0 < 4 or x1 >= self.w - 4 or y1 >= self.h - 4:
            return False
        return not any(self.taken[y * self.w + x] for y in range(y0, y1 + 1) for x in range(x0, x1 + 1))

    def take(self, box):
        x0, y0, x1, y1 = self.cells(box)
        for y in range(max(0, y0), min(self.h, y1 + 1)):
            for x in range(max(0, x0), min(self.w, x1 + 1)):
                self.taken[y * self.w + x] = 1

    def near(self, fp, x, y):
        """Put fp at the free place nearest (x, y) mm, upright or turned."""
        for r in range(0, 400):
            for dx, dy in ring(r):
                for deg in (0, 90):
                    fp.SetOrientationDegrees(deg)
                    fp.SetPosition(pcbnew.VECTOR2I(pcbnew.FromMM(x + dx * self.STEP * 2),
                                                   pcbnew.FromMM(y + dy * self.STEP * 2)))
                    box = self.box(fp)
                    if self.fits(box):
                        self.take(box)
                        return True
        return False


def ring(r):
    """The grid points at Chebyshev distance r, nearest the axes first."""
    if r == 0:
        return [(0, 0)]
    pts = [(dx, dy) for dx in range(-r, r + 1) for dy in (-r, r)]
    pts += [(dx, dy) for dx in (-r, r) for dy in range(-r + 1, r)]
    return sorted(pts, key=lambda p: abs(p[0]) + abs(p[1]))


# The connectors to the outside along the left edge, top to bottom - the
# module slot, the monitor, С2 (the keyboard's socket XS2 when it has a
# footprint) - and the printer's at the left of the bottom edge, each D-sub
# facing out, its nearer row of pins the footprint's 9.40 mm from the edge;
# the ones inside the case (supply, drive, МС 7007, the speaker, SA1) in
# the margin beside them.
EDGE = ["XS1", "XP6", "XP5", "XS2"]
BOTTOM = ["XP3"]
INSIDE = ["XP2", "XP4", "XP8", "XP1", "XP7"]
DSUB_EDGE_MM = 9.40
POWER_NETS = ("GND", "+5V", "+12V", "-12V")


def place_dips(fps, free):
    """The chips and packs where the 1991 board had them; a socket where
    its chip stood."""
    for ref, (x, y) in originals().items():
        fp = fps.get(bom.SOCKETS.get(ref, ref))
        if fp is None:
            continue
        fp.SetOrientationDegrees(DIP_ROTATION)
        fp.SetPosition(pcbnew.VECTOR2I(pcbnew.FromMM(x), pcbnew.FromMM(y)))
        free.take(free.box(fp))
        fp.SetLocked(True)


def to_edge(fp, y):
    """A D-sub facing the left edge: turned so its shell points left (at 0
    degrees it points down), its nearer row of pins DSUB_EDGE_MM from the
    edge, its top at y."""
    fp.SetOrientationDegrees(270)
    fp.SetPosition(pcbnew.VECTOR2I(0, 0))
    left = min(pad.GetPosition().x for pad in fp.Pads() if pad.GetNumber())
    top = pcbnew.FromMM(y) - fp.GetBoundingBox(False).GetTop()
    fp.SetPosition(pcbnew.VECTOR2I(pcbnew.FromMM(DSUB_EDGE_MM) - left, top))


def to_bottom(fp, x):
    """A D-sub facing the bottom edge, its left end at x."""
    fp.SetOrientationDegrees(0)
    fp.SetPosition(pcbnew.VECTOR2I(0, 0))
    low = max(pad.GetPosition().y for pad in fp.Pads() if pad.GetNumber())
    left = pcbnew.FromMM(x) - fp.GetBoundingBox(False).GetLeft()
    fp.SetPosition(pcbnew.VECTOR2I(left, pcbnew.FromMM(BOARD_H - DSUB_EDGE_MM) - low))


def place_edge(fps, free):
    """The outside connectors down the left edge, the inside ones beside."""
    y = 8.0
    for ref in EDGE:
        fp = fps.get(ref)
        if fp is None:
            continue
        if ref == "XS1":
            fp.SetOrientationDegrees(0)
            fp.SetPosition(pcbnew.VECTOR2I(0, 0))
            box = fp.GetBoundingBox(False)
            fp.SetPosition(pcbnew.VECTOR2I(pcbnew.FromMM(3.0) - box.GetLeft(), pcbnew.FromMM(y) - box.GetTop()))
        else:
            to_edge(fp, y)
        box = free.box(fp)
        free.take(box)
        fp.SetLocked(True)
        y = box[3] + 3.0
    for ref in BOTTOM:
        fp = fps.get(ref)
        if fp is not None:
            to_bottom(fp, 30.0)
            free.take(free.box(fp))
            fp.SetLocked(True)
    y = 8.0
    for ref in INSIDE + ["SA1"]:
        fp = fps.get(ref)
        if fp is not None and not free.near(fp, 38.0, y):
            raise SystemExit(f"{ref}: no place on the left")
        if fp is not None:
            y = free.box(fp)[3] + 3.0


def anchor(ref, nets, chips):
    """The chip a part works for: the one it shares most signal nets with."""
    mine = {net for (r, _), net in nets.items() if r == ref and net not in POWER_NETS}
    best, score = None, 0
    for chip, chip_nets in chips.items():
        n = len(mine & chip_nets)
        if n > score:
            best, score = chip, n
    return best


def place_rest(fps, free, nets):
    """Every other part next to its chip; the decoupling capacitors (on
    power alone) one to a chip, in the chips' order on the board."""
    placed = {ref for ref, fp in fps.items() if fp.IsLocked() or ref in EDGE + BOTTOM + INSIDE + ["SA1"]}
    chips = defaultdict(set)
    for (ref, _), net in nets.items():
        if bom.is_chip(ref) or ref in bom.SOCKETS.values():
            if net not in POWER_NETS:
                chips[ref].add(net)
    order = sorted(chips, key=lambda r: (fps[r].GetPosition().y, fps[r].GetPosition().x) if r in fps else (0, 0))
    spare = iter(order * 2)
    failed = []
    for ref in sorted(fps, key=bom.natural):
        if ref in placed:
            continue
        fp = fps[ref]
        home = anchor(ref, nets, chips) or next(spare)
        target = fps[home].GetBoundingBox(False).GetCenter() if home in fps else pcbnew.VECTOR2I(0, 0)
        if not free.near(fp, pcbnew.ToMM(target.x), pcbnew.ToMM(target.y)):
            failed.append(ref)
    return failed


def outline(board):
    corners = [(0, 0), (BOARD_W, 0), (BOARD_W, BOARD_H), (0, BOARD_H)]
    for (x0, y0), (x1, y1) in zip(corners, corners[1:] + corners[:1]):
        seg = pcbnew.PCB_SHAPE(board)
        seg.SetShape(pcbnew.SHAPE_T_SEGMENT)
        seg.SetStart(pcbnew.VECTOR2I(pcbnew.FromMM(x0), pcbnew.FromMM(y0)))
        seg.SetEnd(pcbnew.VECTOR2I(pcbnew.FromMM(x1), pcbnew.FromMM(y1)))
        seg.SetLayer(pcbnew.Edge_Cuts)
        seg.SetWidth(pcbnew.FromMM(0.1))
        board.Add(seg)


def plane(board, codes, net, layer):
    zone = pcbnew.ZONE(board)
    zone.SetLayer(layer)
    zone.SetNet(codes[net])
    zone.SetIsFilled(False)
    poly = zone.Outline()
    poly.NewOutline()
    for x, y in ((0.5, 0.5), (BOARD_W - 0.5, 0.5), (BOARD_W - 0.5, BOARD_H - 0.5), (0.5, BOARD_H - 0.5)):
        poly.Append(pcbnew.FromMM(x), pcbnew.FromMM(y))
    board.Add(zone)


# The design rules: a common fab's four-layer process with room to spare
# (0.25 mm tracks, 0.2 mm gaps, 0.7/0.35 mm vias); the supplies wider.
SIGNAL = {"track": 0.25, "clearance": 0.2, "via": 0.7, "drill": 0.35}
POWER = {"track": 0.6, "clearance": 0.25, "via": 0.9, "drill": 0.45}


def netclass(nc, rule):
    nc.SetTrackWidth(pcbnew.FromMM(rule["track"]))
    nc.SetClearance(pcbnew.FromMM(rule["clearance"]))
    nc.SetViaDiameter(pcbnew.FromMM(rule["via"]))
    nc.SetViaDrill(pcbnew.FromMM(rule["drill"]))


def rules(board):
    settings = board.GetDesignSettings().m_NetSettings
    netclass(settings.GetDefaultNetclass(), SIGNAL)
    power = pcbnew.NETCLASS("Power")
    netclass(power, POWER)
    settings.SetNetclass("Power", power)
    for net in POWER_NETS:
        settings.SetNetclassPatternAssignment(net, "Power")


def build():
    board = pcbnew.BOARD()
    board.SetCopperLayerCount(4)
    rules(board)
    dirs = footprint_dirs()
    nets = pin_nets()
    codes, fps, skipped, missing = {}, {}, [], []
    for ref, item in sorted(load_items().items(), key=lambda kv: bom.natural(kv[0])):
        fp = new_footprint(board, dirs, item)
        if fp is None:
            skipped.append(f"{ref} ({item.footprint})")
            continue
        fps[ref] = fp
        missing += connect(board, fp, item.type, nets, codes)
    free = Free(board)
    outline(board)
    place_dips(fps, free)
    place_edge(fps, free)
    failed = place_rest(fps, free, nets)
    plane(board, codes, "GND", pcbnew.In1_Cu)
    plane(board, codes, "+5V", pcbnew.In2_Cu)
    return board, skipped, missing, failed


def board_nets(board):
    """{(ref, pad): net} as the board has them."""
    return {(fp.GetReference(), pad.GetNumber()): pad.GetNetname()
            for fp in board.GetFootprints() for pad in fp.Pads() if pad.GetNumber()}


def check(board):
    """The pads' nets against the netlist: a list of differences."""
    want = {}
    kinds = {ref: item.type for ref, item in load_items().items()}
    for (ref, pin), net in pin_nets().items():
        if ref in kinds:
            want[(ref, pad_of(kinds[ref], pin))] = net
    have = board_nets(board)
    out = [f"{r}.{p}: the netlist's {n}, the board's {have.get((r, p), 'no pad')}"
           for (r, p), n in sorted(want.items()) if have.get((r, p)) != n and (r, p) in have]
    out += [f"{r}.{p}: on {n} on the board, on no net in the netlist"
            for (r, p), n in sorted(have.items()) if n and (r, p) not in want]
    return out


def main(argv):
    if argv[1:] == ["check"]:
        diffs = check(pcbnew.LoadBoard(OUT))
        for d in diffs:
            print(d)
        print(f"{len(diffs)} differences")
        return 1 if diffs else 0
    board, skipped, missing, failed = build()
    for s in skipped:
        print("no footprint yet:", s)
    for m in missing:
        print("no pad for", m)
    for f in failed:
        print("no place for", f)
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    board.Save(OUT)
    print(f"{len(board.GetFootprints())} footprints, {board.GetNetCount()} nets -> {OUT}")
    return 1 if missing or failed else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
