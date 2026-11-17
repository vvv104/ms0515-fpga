#!/usr/bin/env python3
"""The memory expansion module as a printed board made anew (docs/plan.md,
6a step 5), for KiCad 10.

Run with KiCad's own Python, which has pcbnew:

    <KiCad>/bin/python tools/pcb_ext.py          - write kicad/ext.kicad_pcb:
                                                   the board placed, no wires
    <KiCad>/bin/python tools/pcb_ext.py check    - the board's pads against the
                                                   module's netlist, exit 1 on
                                                   a difference

The board is the module's netlist (netlist/ext/: parts.csv, nets.csv, the
contacts its board leaves free, pcb.csv) with the footprints of
netlist/ext/packages.csv and the chips' DIPs (tools/bom.py).

It is the original module's board as its photos show it (owner's module,
dated 09.11.93; $MS0515_DATA/photos/module-holes: every hole read off both
sides, the owner's corrections, the chips' rows fitted on their DIPs):
246.1 x 71.1 mm, every chip, socket and empty footprint on its holes
(netlist/ext/placement.csv, pin 1 of each).  Four copper layers as the
original's (thermal crosses on the RAM's supply pins, inner copper showing
through the laminate; owner, 2026-11-17): signals outside, ground and +5 V
on the planes inside (PLANES).  It stands on НС4 as the original did
(owner, 2026-11-17; $MS0515_DATA/photos/known-machines/c0975acce323.jpg):
on edge along НС4's back edge, its lower edge on НС4 (or 1 mm over it),
the parts towards the back wall, the solder side towards XS1, the board
passing behind XS1 down to НС4; the plug on the solder side over XS1, its
face above the board's lower edge.  One notch in the lower edge, over
НС4's switches SA1 (NOTCH).  The plug is DIN 41612 C, angled, male (НС4's
XS1 made anew is the straight female) where the original had an СНП59, on
the original's holes: a1 and row a where its photos have them.  The
mounting holes as the original's.
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
import pcb  # noqa: E402

ROOT = os.path.dirname(HERE)
EXT = os.path.join(ROOT, "netlist", "ext")
OUT = os.path.join(ROOT, "kicad", "ext.kicad_pcb")
BOARD_W, BOARD_H = 246.1, 71.1           # the original's, its photos' edges (+-0.5 mm); the
                                          # upper one 0.5 mm higher: R13's holes 0.9 mm under it
SOCKETS = {"D1": "XS2", "D2": "XS3", "D3": "XS4"}    # parts.csv: XS2..XS4, «DIP socket for D1..D3»

# The plug: a1 and row a where the original's are (its photos).  Its face
# then stands 15.35 - 6.4 (row a to the face, Harting's angled male C) =
# 8.95 mm over the lower edge: a DIN 41612 C female's height on НС4.
A1_X, ROW_A_Y = 43.35, 55.75
ROW_A_TO_FACE = 6.4
# The notch over SA1 (its photos: 20.4..30.8; SA1 5 mm high).
NOTCH, NOTCH_DEPTH = (20.4, 30.8), 9.0
# The original's mounting holes (its photos; the owner's marks), about 3 mm.
HOLES = {"H1": (4.8, 4.8), "H2": (63.9, 4.9)}
HOLE_DRILL = 3.2                          # kicad/ns4.pretty: an M3 hole

OUTLINE = [(0, 0), (BOARD_W, 0), (BOARD_W, BOARD_H), (NOTCH[1], BOARD_H),
           (NOTCH[1], BOARD_H - NOTCH_DEPTH), (NOTCH[0], BOARD_H - NOTCH_DEPTH),
           (NOTCH[0], BOARD_H), (0, BOARD_H)]
CUTS = {"SA1": (NOTCH[0], BOARD_H - NOTCH_DEPTH, NOTCH[1], BOARD_H)}
MARGIN = 0.5                              # mm from a courtyard to the board's edge
# The stack: the planes inside on layers of type power (as НС4's, tools/pcb.py).
PLANES = {pcbnew.In1_Cu: "GND", pcbnew.In2_Cu: "+5V"}
# The rules: two tracks between two pins of a DIP (0.2 + 0.15 gaps in
# 0.94 mm); the supplies' stubs to the planes wider.
SIGNAL = {"track": 0.2, "clearance": 0.15, "via": 0.6, "drill": 0.3}
POWER = {"track": 0.5, "clearance": 0.15, "via": 0.8, "drill": 0.4}
# ±12 V feed the RS-232 drivers and pull-ups, a few mA: narrower.
BIAS = {"track": 0.3, "clearance": 0.15, "via": 0.6, "drill": 0.3}
BIAS_NETS = ("+12V", "-12V")


def inside(box):
    """A courtyard (x0, y0, x1, y1) on the board, clear of its edge and cuts."""
    x0, y0, x1, y1 = box
    if x0 < MARGIN or y0 < MARGIN or x1 > BOARD_W - MARGIN or y1 > BOARD_H - MARGIN:
        return False
    return not any(x0 < cx1 + MARGIN and cx0 - MARGIN < x1 and y1 > cy0 - MARGIN
                   for cx0, cy0, cx1, _ in CUTS.values())


def pin_nets():
    """{(ref, pin): net} with the chips' power pins, the board's changes
    (netlist/ext/pcb.csv) applied; a socketed chip's pins given to its socket."""
    parts, _, conns, _ = nl.load_module(EXT)
    conns, errors = nl.apply_changes(conns, nl.load_changes(os.path.join(EXT, "pcb.csv")))
    if errors:
        raise SystemExit("\n".join(p.text for p in errors))
    nets = {}
    for net, pins in kicad.nets_of(parts, conns).items():
        for ref, pin in sorted(pins):
            nets[(SOCKETS.get(ref, ref), int(pin))] = net
    return nets


def assembly():
    """The parts the board carries: the RAM disk as the owner's module has
    them (netlist/ext/assembly.csv; owner, 2026-11-17)."""
    return {r["ref"] for r in nl.rows(os.path.join(EXT, "assembly.csv"))}


def load_items():
    """{ref: Item} of the parts placed: the RAM disk's (assembly()); a socketed
    chip is not, its socket is."""
    found, problems = bom.items(os.path.join(EXT, "parts.csv"), os.path.join(EXT, "packages.csv"),
                                os.path.join(ROOT, "docs", "chips.md"), os.devnull)
    if problems:
        raise SystemExit("\n".join(problems))
    keep = assembly()
    for ref, item in found.items():
        item.placed = ref not in SOCKETS and ref in keep
    return {ref: item for ref, item in found.items() if item.placed}


def placement():
    """{ref: (x, y, rotation)}: netlist/ext/placement.csv."""
    return {r["ref"]: (float(r["x"]), float(r["y"]), float(r["rotation"]))
            for r in nl.rows(os.path.join(EXT, "placement.csv"))}


def copper():
    """[{kind, layer, net, x0, y0, x1, y1, width}]: netlist/ext/copper.csv,
    the original's copper its photos show for sure."""
    return nl.rows(os.path.join(EXT, "copper.csv"))


def lay_copper(board):
    """The photos' copper on the board, locked: route.py's Freerouting
    leaves it where it is and routes the rest."""
    at = lambda x, y: pcbnew.VECTOR2I(pcbnew.FromMM(float(x)), pcbnew.FromMM(float(y)))
    layers = {"F.Cu": pcbnew.F_Cu, "B.Cu": pcbnew.B_Cu}
    for r in copper():
        net = board.FindNet(r["net"])
        if r["kind"] == "via":
            v = pcbnew.PCB_VIA(board)
            v.SetPosition(at(r["x0"], r["y0"]))
            v.SetLayerPair(pcbnew.F_Cu, pcbnew.B_Cu)
            v.SetWidth(pcbnew.FromMM(SIGNAL["via"]))
            v.SetDrill(pcbnew.FromMM(SIGNAL["drill"]))
            item = v
        else:
            item = pcbnew.PCB_TRACK(board)
            item.SetStart(at(r["x0"], r["y0"]))
            item.SetEnd(at(r["x1"], r["y1"]))
            item.SetWidth(pcbnew.FromMM(float(r["width"])))
            item.SetLayer(layers[r["layer"]])
        item.SetNet(net)
        item.SetLocked(True)
        board.Add(item)


def box_of(fp):
    """The courtyard's box, mm."""
    fp.BuildCourtyardCaches()
    layer = pcbnew.B_CrtYd if fp.IsFlipped() else pcbnew.F_CrtYd
    shape = fp.GetCourtyard(layer)
    b = shape.BBox() if shape.OutlineCount() else fp.GetBoundingBox(False, False)
    return pcbnew.ToMM(b.GetLeft()), pcbnew.ToMM(b.GetTop()), pcbnew.ToMM(b.GetRight()), pcbnew.ToMM(b.GetBottom())


def stand(fp, x, y, rotation):
    """fp turned and moved so that its pad 1 is at (x, y)."""
    fp.SetOrientationDegrees(rotation)
    one = fp.FindPadByNumber("1").GetPosition()
    at = fp.GetPosition()
    fp.SetPosition(pcbnew.VECTOR2I(at.x + pcbnew.FromMM(x) - one.x, at.y + pcbnew.FromMM(y) - one.y))


def place_plug(fp):
    """The plug on the solder side, its face downwards: a1 at (A1_X,
    ROW_A_Y), a1..a32 to the right, rows b and c above row a; without the
    footprint's two mounting holes, which would cut the original's tracks."""
    for pad in [p for p in fp.Pads() if not p.GetNumber()]:
        fp.Remove(pad)
    fp.SetOrientationDegrees(180)
    fp.Flip(fp.GetPosition(), pcbnew.FLIP_DIRECTION_LEFT_RIGHT)
    a1 = fp.FindPadByNumber("a1").GetPosition()
    at = fp.GetPosition()
    fp.SetPosition(pcbnew.VECTOR2I(at.x + pcbnew.FromMM(A1_X) - a1.x, at.y + pcbnew.FromMM(ROW_A_Y) - a1.y))


class Free(pcb.Free):
    """The board's place in 0.5 mm cells: what is off the board, the plug's
    body and every courtyard taken."""

    def __init__(self):
        self.w, self.h = int(BOARD_W / self.STEP), int(BOARD_H / self.STEP)
        self.taken = bytearray(self.w * self.h)
        for y in range(self.h):
            for x in range(self.w):
                cell = (x * self.STEP, y * self.STEP, (x + 1) * self.STEP, (y + 1) * self.STEP)
                if not inside(cell):
                    self.taken[y * self.w + x] = 1

    def fits(self, box):
        x0, y0, x1, y1 = self.cells(box)
        if x0 < 0 or y0 < 0 or x1 >= self.w or y1 >= self.h:
            return False
        return not any(self.taken[y * self.w + x] for y in range(y0, y1 + 1) for x in range(x0, x1 + 1))

    def box(self, fp):
        return box_of(fp)


def place(board, dirs, fps, free):
    """The chips and connectors where placement.csv puts them, the plug and
    the holes; returns those placement.csv names that the board has not."""
    wrong = []
    for ref, (x, y, rotation) in placement().items():
        fp = fps.get(ref)
        if fp is None:
            wrong.append(ref)
            continue
        stand(fp, x, y, rotation)
        free.take(box_of(fp))
        fp.SetLocked(True)
    place_plug(fps["XP1"])
    free.take(box_of(fps["XP1"]))           # a part's leads would come through onto the plug's body
    fps["XP1"].SetLocked(True)
    for ref, (x, y) in HOLES.items():
        fp = fps[ref] = pcbnew.FootprintLoad(dirs["ns4"], f"Hole_{HOLE_DRILL:.1f}mm_NPTH")
        fp.SetReference(ref)
        fp.SetPosition(pcbnew.VECTOR2I(pcbnew.FromMM(x), pcbnew.FromMM(y)))
        board.Add(fp)
        free.take(box_of(fp))
        fp.SetLocked(True)
    return wrong


def place_rest(fps, free, nets):
    """Every other part beside the chip it shares most signals with; the
    decoupling capacitors (on the supplies alone) one to a chip in turn."""
    chips = defaultdict(set)
    for (ref, _), net in nets.items():
        if (bom.is_chip(ref) or ref in SOCKETS.values()) and net not in pcb.POWER_NETS:
            chips[ref].add(net)
    order = sorted(chips, key=lambda r: (fps[r].GetPosition().x, fps[r].GetPosition().y) if r in fps else (0, 0))
    spare = iter(order * 2)
    failed = []
    for ref in sorted(fps, key=bom.natural):
        fp = fps[ref]
        if fp.IsLocked():
            continue
        home = pcb.anchor(ref, nets, chips) or next(spare)
        x0, y0, x1, y1 = box_of(fps[home])
        if not free.near(fp, (x0 + x1) / 2, (y0 + y1) / 2):
            failed.append(ref)
    return failed


def outline(board):
    for (x0, y0), (x1, y1) in zip(OUTLINE, OUTLINE[1:] + OUTLINE[:1]):
        seg = pcbnew.PCB_SHAPE(board)
        seg.SetShape(pcbnew.SHAPE_T_SEGMENT)
        seg.SetStart(pcbnew.VECTOR2I(pcbnew.FromMM(x0), pcbnew.FromMM(y0)))
        seg.SetEnd(pcbnew.VECTOR2I(pcbnew.FromMM(x1), pcbnew.FromMM(y1)))
        seg.SetLayer(pcbnew.Edge_Cuts)
        seg.SetWidth(pcbnew.FromMM(0.1))
        board.Add(seg)


def stack(board):
    """Four copper layers, the planes' of type power."""
    board.SetCopperLayerCount(4)
    for layer in board.GetEnabledLayers().CuStack():
        board.SetLayerType(layer, pcbnew.LT_POWER if layer in PLANES else pcbnew.LT_SIGNAL)


def planes(board):
    """Each plane over the whole outline, unfilled (KiCad fills it)."""
    for layer, net in PLANES.items():
        zone = pcbnew.ZONE(board)
        zone.SetLayer(layer)
        zone.SetNet(board.FindNet(net))
        zone.SetIsFilled(False)
        poly = zone.Outline()
        poly.NewOutline()
        for x, y in OUTLINE:
            poly.Append(pcbnew.FromMM(x), pcbnew.FromMM(y))
        board.Add(zone)


def rules(board):
    """НС4's edge and least width (tools/pcb.py), this board's classes."""
    board.GetDesignSettings().m_CopperEdgeClearance = pcbnew.FromMM(pcb.EDGE_CLEARANCE)
    board.GetDesignSettings().m_TrackMinWidth = pcbnew.FromMM(pcb.MIN_TRACK_MM)
    settings = board.GetDesignSettings().m_NetSettings
    pcb.netclass(settings.GetDefaultNetclass(), SIGNAL)
    for name, rule, nets in (("Power", POWER, ("+5V", "GND")), ("Bias", BIAS, BIAS_NETS)):
        nc = pcbnew.NETCLASS(name)
        pcb.netclass(nc, rule)
        settings.SetNetclass(name, nc)
        for net in nets:
            settings.SetNetclassPatternAssignment(net, name)


def build():
    """The board placed, no wires: the pads on the module's nets."""
    board = pcbnew.BOARD()
    stack(board)
    rules(board)
    dirs = pcb.footprint_dirs()
    nets = pin_nets()
    codes, fps, skipped, missing = {}, {}, [], []
    for ref, item in sorted(load_items().items(), key=lambda kv: bom.natural(kv[0])):
        fp = pcb.new_footprint(board, dirs, item)
        if fp is None:
            skipped.append(f"{ref} ({item.footprint})")
            continue
        fps[ref] = fp
        missing += pcb.connect(board, fp, item.type, nets, codes)
    outline(board)
    free = Free()
    missing += [f"{ref}: in placement.csv, not on the board" for ref in place(board, dirs, fps, free)]
    failed = place_rest(fps, free, nets)
    pcb.snap(fps)
    planes(board)
    lay_copper(board)
    return board, skipped, missing, failed


def check(board):
    """The pads' nets against the module's netlist: a list of differences."""
    kinds = {ref: item.type for ref, item in load_items().items()}
    want = {(ref, pcb.pad_of(kinds[ref], pin)): net for (ref, pin), net in pin_nets().items() if ref in kinds}
    have = pcb.board_nets(board)
    out = [f"{r}.{p}: the netlist's {n}, the board's {have.get((r, p), 'no pad')}"
           for (r, p), n in sorted(want.items()) if have.get((r, p)) != n]
    out += [f"{r}.{p}: on {n} on the board, on no net in the netlist"
            for (r, p), n in sorted(have.items()) if n and (r, p) not in want]
    return out


def main(argv):
    if argv[1:2] == ["check"]:
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
    board.Save(OUT)
    print(f"{len(board.GetFootprints())} footprints, {board.GetNetCount()} nets -> {OUT}")
    return 1 if skipped or missing or failed else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
