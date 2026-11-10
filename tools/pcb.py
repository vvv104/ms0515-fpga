#!/usr/bin/env python3
"""НС4 as a printed board made anew (docs/plan.md stage 8d), for KiCad 10.

Run with KiCad's own Python, which has pcbnew:

    <KiCad>/bin/python tools/pcb.py [vvv104] - write kicad/ns4.kicad_pcb, or
                                               kicad/ns4-vvv104.kicad_pcb:
                                               the board placed, no wires
    <KiCad>/bin/python tools/pcb.py check [vvv104]
                                             - the board's pads against the
                                               netlist (stage 8e), exit 1 on
                                               a difference
    <KiCad>/bin/python tools/pcb.py derive TO [FROM]
                                             - the board TO made from the
                                               board FROM as routed (ns4 for
                                               Э3's, or a variant's name; FROM
                                               ns4 when left out): the pins
                                               that differ put on TO's nets,
                                               the nets they left unrouted

Two boards, one placement: ns4 is Э3 as drawn (the one whose 640 x 200
left edge is still open, docs/findings.md), ns4-vvv104 the owner's board as
it worked, D70.15 on GND.  Every part stands where Э3's nets put it on
both, so one is made from the other as routed (`derive`), and the two
differ only in the changed nets' copper.

The board is the netlist's (netlist/parts.csv, nets.csv - Э3 as read, the
one the board model runs the machine's software on) with the footprints of
tools/bom.py.  Eight copper layers, as the 1991 board had (owner,
2026-11-09): signals on F.Cu and In1.Cu (its layers 1 and 2, on Mick's
traces), In3.Cu and In4.Cu, In6.Cu and B.Cu (its 7 and 8); ground on
In2.Cu and +5 V on In5.Cu as planes.  The 1991 board's layers 3 to 6 are
on no scan; some of its pins join only there (netlist/copper.json), so it
had signals inside too.  The plane layers are of type power (`PLANES`,
`stack`), so that KiCad's Specctra export names them `(type power)` and
Freerouting routes no signal over them (given as signal layers, as the
four-layer board of 2026-11-05 was, it ran more than half the track
through the planes).  Six layers - four of signals - left 309 of 1258
connections after two days (docs/baseline.md step 8).  Every part stands
where it stood on the 1991 board (owner, 2026-11-10): the chips and the
resistor packs by pin 1 (netlist/placement.csv, on Mick's layer traces,
2.54 mm a 39.4 pixels), every other part by its pads on the 1991 holes
(netlist/placement-1991.csv, read off the assembly drawing; their own
footprints from tools/pretty.py where the 1991 holes are not a library
footprint's).  Only the board's own addition, E9, goes into the free place
nearest the chip it works for.  Routing is Freerouting's (tools/route.py).
"""
import json
import math
import os
import sys
from collections import defaultdict

import pcbnew

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import bom  # noqa: E402
import kicad  # noqa: E402
import lay1991  # noqa: E402
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


def board_path(variant=None):
    """kicad/ns4.kicad_pcb, Э3's board; kicad/ns4-<variant>.kicad_pcb, one with
    netlist/<variant>.csv's changes."""
    return OUT if not variant else os.path.join(ROOT, "kicad", f"ns4-{variant}.kicad_pcb")


def changes(variant):
    """netlist/<variant>.csv's changes, none for Э3's board."""
    return nl.load_changes(os.path.join(NET, f"{variant}.csv")) if variant else []


def pin_nets(variant=None):
    """{(ref, pin): net} with the chips' power pins; a chip in a socket's
    pins given to the socket.  With a variant its changes are applied."""
    parts, _, conns = nl.load(os.path.join(NET, "parts.csv"), os.path.join(NET, "pinouts.csv"),
                              os.path.join(NET, "nets.csv"))
    parts.update(nl.load_parts(os.path.join(NET, "pcb_parts.csv")))
    conns += nl.load_nets(os.path.join(NET, "pcb_nets.csv"))
    conns, errors = nl.apply_changes(conns, nl.load_changes(os.path.join(NET, "pcb.csv")))
    if not errors:
        conns, errors = nl.apply_changes(conns, changes(variant))
    if errors:
        raise SystemExit("\n".join(p.text for p in errors))
    nets = {}
    for net, pins in kicad.nets_of(parts, conns).items():
        for ref, pin in sorted(pins):       # sets follow the process's string hashing: a
            nets[(bom.SOCKETS.get(ref, ref), int(pin))] = net    # placement must not
    return nets


def load_items():
    """Э3's parts and the board's own additions (netlist/pcb_parts.csv)."""
    found, problems = {}, []
    for parts in ("parts.csv", "pcb_parts.csv"):
        more, wrong = bom.items(os.path.join(NET, parts), os.path.join(NET, "packages.csv"),
                                os.path.join(ROOT, "docs", "chips.md"), os.path.join(NET, "firmware.csv"))
        found.update(more)
        problems += wrong
    if problems:
        raise SystemExit("\n".join(problems))
    return {ref: item for ref, item in found.items() if item.placed}


def added():
    """The refs of the board's own additions: placed after Э3's parts, so
    that Э3's stand where they would without them."""
    return {r.ref for r in nl.load_parts(os.path.join(NET, "pcb_parts.csv")).values()}


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


POWER_NETS = ("GND", "+5V", "+12V", "-12V")
# A pad farther than this from its 1991 hole is an error: the holes stand
# +-1 px (0.06 mm) as Mick's traces draw them.  XS2 keeps the real DIN
# socket's 2.5 mm grid on a board laid out at 2.54 mm a 1991 grid step.
MISS_MM = 0.15
MISS_OWN_GRID_MM = {"XS2": 0.25}


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


def holes_1991():
    """{ref: {pad: (x, y) mm}}: every pad of every part but the DIPs where
    the 1991 board had it (netlist/placement-1991.csv)."""
    out = defaultdict(dict)
    for r in nl.rows(os.path.join(NET, "placement-1991.csv")):
        out[r["ref"]][r["pad"]] = (float(r["x"]) * MM_PER_PX, float(r["y"]) * MM_PER_PX)
    return out


def put(fp, want):
    """Turn (by quarters) and move fp so that its pads stand on want
    ({pad: (x, y) mm}); the worst pad's miss, mm."""
    best = None
    for rotation in (0, 90, 180, 270):
        fp.SetOrientationDegrees(rotation)
        fp.SetPosition(pcbnew.VECTOR2I(0, 0))
        have = {p.GetNumber(): (pcbnew.ToMM(p.GetPosition().x), pcbnew.ToMM(p.GetPosition().y))
                for p in fp.Pads() if p.GetNumber() in want}
        if not have:
            continue
        dx = sum(want[n][0] - x for n, (x, _) in have.items()) / len(have)
        dy = sum(want[n][1] - y for n, (_, y) in have.items()) / len(have)
        miss = max(math.hypot(x + dx - want[n][0], y + dy - want[n][1]) for n, (x, y) in have.items())
        if best is None or miss < best[0]:
            best = (miss, rotation, dx, dy)
    miss, rotation, dx, dy = best
    fp.SetOrientationDegrees(rotation)
    fp.SetPosition(pcbnew.VECTOR2I(pcbnew.FromMM(dx), pcbnew.FromMM(dy)))
    return miss


def board_holes():
    """{ref: drill mm} of the 1991 board's own holes H1..H10 (placement-1991.csv: "circle D mm")."""
    return {r["ref"]: r["source"].split()[1] for r in nl.rows(os.path.join(NET, "placement-1991.csv"))
            if r["ref"].startswith("H") and r["ref"][1:].isdigit()}


def mounting_hole(board, dirs, ref, drill):
    """One of the 1991 board's holes, a footprint of no part (tools/pretty.py)."""
    fp = pcbnew.FootprintLoad(dirs["ns4"], f"Hole_{drill}mm_NPTH")
    fp.SetReference(ref)
    board.Add(fp)
    return fp


def place_1991(board, dirs, fps, free):
    """Every part but the DIPs on its 1991 holes, locked; the board's own
    holes where the 1991 board has them."""
    holes = board_holes()
    for ref, want in holes_1991().items():
        fp = fps.get(ref)
        if fp is None and ref in holes:
            fp = fps[ref] = mounting_hole(board, dirs, ref, holes[ref])
        if fp is None:
            continue
        miss = put(fp, want)
        if miss > MISS_OWN_GRID_MM.get(ref, MISS_MM):
            raise SystemExit(f"{ref}: its footprint misses the 1991 holes by {miss:.2f} mm")
        free.take(free.box(fp))
        fp.SetLocked(True)


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
    placed = {ref for ref, fp in fps.items() if fp.IsLocked()}
    chips = defaultdict(set)
    for (ref, _), net in nets.items():
        if bom.is_chip(ref) or ref in bom.SOCKETS.values():
            if net not in POWER_NETS:
                chips[ref].add(net)
    order = sorted(chips, key=lambda r: (fps[r].GetPosition().y, fps[r].GetPosition().x) if r in fps else (0, 0))
    spare = iter(order * 2)
    failed, extra = [], added()
    for ref in sorted(fps, key=lambda r: (r in extra, bom.natural(r))):
        if ref in placed:
            continue
        fp = fps[ref]
        home = anchor(ref, nets, chips) or next(spare)
        target = fps[home].GetBoundingBox(False).GetCenter() if home in fps else pcbnew.VECTOR2I(0, 0)
        if not free.near(fp, pcbnew.ToMM(target.x), pcbnew.ToMM(target.y)):
            failed.append(ref)
    return failed


ROUTING_STEP = 100                        # nm: the Specctra design's resolution (um 10)


def snap(fps):
    """Every part on the routing's 0.1 um grid: a session's wires end on the
    pins at those coordinates, and a board placed again must have its pins
    there too, or the routing carried over (tools/route.py start --keep)
    misses them by a few hundredths of a micron - Freerouting counts such a
    wire unconnected."""
    for fp in fps.values():
        at = fp.GetPosition()
        fp.SetPosition(pcbnew.VECTOR2I(round(at.x / ROUTING_STEP) * ROUTING_STEP,
                                       round(at.y / ROUTING_STEP) * ROUTING_STEP))


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


# The design rules: a common fab's multilayer process with room to spare
# (0.25 mm tracks, 0.2 mm gaps, 0.7/0.35 mm vias, 0.5 mm to the board's
# edge); the supplies wider, at the same gap - the 1991 copper runs 0.2 mm
# from the supply pins (Mick's traces).
SIGNAL = {"track": 0.25, "clearance": 0.2, "via": 0.7, "drill": 0.35}
POWER = {"track": 0.6, "clearance": 0.2, "via": 0.9, "drill": 0.45}
EDGE_CLEARANCE = 0.5                      # mm; tools/route.py gives Freerouting the same
MIN_TRACK_MM = 0.15                       # the 1991 runs are 0.19 mm (TRACK_1991_MM), a fab's 6 mil

# The stack: the planes inside, on layers of type power - what the Specctra
# export writes as `(type power)`, which keeps Freerouting's signals off them.
PLANES = {pcbnew.In2_Cu: "GND", pcbnew.In5_Cu: "+5V"}
COPPER_LAYERS = 8


def netclass(nc, rule):
    nc.SetTrackWidth(pcbnew.FromMM(rule["track"]))
    nc.SetClearance(pcbnew.FromMM(rule["clearance"]))
    nc.SetViaDiameter(pcbnew.FromMM(rule["via"]))
    nc.SetViaDrill(pcbnew.FromMM(rule["drill"]))


def rules(board):
    board.GetDesignSettings().m_CopperEdgeClearance = pcbnew.FromMM(EDGE_CLEARANCE)
    board.GetDesignSettings().m_TrackMinWidth = pcbnew.FromMM(MIN_TRACK_MM)
    settings = board.GetDesignSettings().m_NetSettings
    netclass(settings.GetDefaultNetclass(), SIGNAL)
    power = pcbnew.NETCLASS("Power")
    netclass(power, POWER)
    settings.SetNetclass("Power", power)
    for net in POWER_NETS:
        settings.SetNetclassPatternAssignment(net, "Power")


def stack(board):
    """Eight copper layers, the plane layers of type power, the rest signal."""
    board.SetCopperLayerCount(COPPER_LAYERS)
    for layer in board.GetEnabledLayers().CuStack():
        board.SetLayerType(layer, pcbnew.LT_POWER if layer in PLANES else pcbnew.LT_SIGNAL)


def build(variant=None):
    """The board placed, no wires: the pads on the variant's nets, the
    parts where Э3's nets put them - one placement for every variant."""
    board = pcbnew.BOARD()
    stack(board)
    rules(board)
    dirs = footprint_dirs()
    nets = pin_nets(variant)
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
    place_1991(board, dirs, fps, free)
    failed = place_rest(fps, free, pin_nets() if variant else nets)
    snap(fps)
    lay_copper(board)
    for layer, net in PLANES.items():
        plane(board, codes, net, layer)
    return board, skipped, missing, failed


# The 1991 board's outer layers (Mick's 1, 2, 7, 8) as fixed copper: tools/lay1991.py.
COPPER_LAYERS_1991 = {"NS4-1": pcbnew.F_Cu, "NS4-2": pcbnew.In1_Cu, "NS4-7": pcbnew.In6_Cu, "NS4-8": pcbnew.B_Cu}
TRACK_1991_MM = 0.19                      # the 1991 signal runs (Mick's traces: 0.193 mm)
WIDE_1991_PX = 8.0                        # wider than this: a supply run, laid at its width
STUB_MM, PAD_R_MM, VIA_R_MM = 2.0, 0.75, 0.5
VIA_1991 = {"via": 1.0, "drill": 0.5}


def hole_nets(board, holes):
    """{hole: net} of the 1991 holes our parts' pads stand on."""
    where = defaultdict(list)
    for i, (x, y) in enumerate(holes):
        where[(int(x // 40), int(y // 40))].append(i)
    out = {}
    for fp in board.GetFootprints():
        for pad in fp.Pads():
            if not pad.GetNetname():
                continue
            x, y = pcbnew.ToMM(pad.GetPosition().x) / MM_PER_PX, pcbnew.ToMM(pad.GetPosition().y) / MM_PER_PX
            near = [i for gx in (-1, 0, 1) for gy in (-1, 0, 1) for i in where[(int(x // 40) + gx, int(y // 40) + gy)]]
            best = min(near, key=lambda i: math.dist((x, y), holes[i]), default=None)
            if best is not None and math.dist((x, y), holes[best]) * MM_PER_PX < 0.3:
                out[best] = pad.GetNetname()
    return out


def group_nets(copper, pad_nets):
    """{hole: net} of the holes no pad takes whose copper group reaches one net."""
    out = {}
    for group in copper["nets"]:
        found = {pad_nets[i] for i in group if i in pad_nets}
        if len(found) == 1:
            out.update({i: next(iter(found)) for i in group if i not in pad_nets})
    return out


def copper_1991(board):
    """The 1991 outer copper as {layer: [[net, width px, pts]]} and its vias {hole: net}."""
    copper = json.load(open(os.path.join(NET, "copper.json"), encoding="utf-8"))
    lines = json.load(open(os.path.join(NET, "lines-1991.json"), encoding="utf-8"))
    holes = copper["pads"]
    pads = hole_nets(board, holes)
    groups = group_nets(copper, pads)
    tracks = lay1991.label_layers({name: lines[name] for name in COPPER_LAYERS_1991}, holes, pads, groups)
    gone = 1
    while gone:                           # a via's net can change as its stubs go
        vias = lay1991.vias(tracks, holes, set(pads))
        radius = {i: (PAD_R_MM if i in pads else VIA_R_MM) / MM_PER_PX for i in set(pads) | set(vias)}
        tracks, gone = lay1991.drop_stubs(tracks, holes, {**vias, **pads}, STUB_MM / MM_PER_PX,
                                          SIGNAL["clearance"] / MM_PER_PX, radius)
    return holes, tracks, lay1991.vias(tracks, holes, set(pads))


def lay_copper(board):
    """Mick's outer layers on the board, locked; Freerouting routes the rest."""
    holes, tracks, vias = copper_1991(board)
    at = lambda p: pcbnew.VECTOR2I(pcbnew.FromMM(p[0] * MM_PER_PX), pcbnew.FromMM(p[1] * MM_PER_PX))
    for name, runs in tracks.items():
        for net, w, pts in runs:
            width = TRACK_1991_MM if w <= WIDE_1991_PX else round(min(w * MM_PER_PX, 2.0) / 0.05) * 0.05
            for a, b in zip(pts, pts[1:]):
                if a != b:
                    t = pcbnew.PCB_TRACK(board)
                    t.SetStart(at(a))
                    t.SetEnd(at(b))
                    t.SetWidth(pcbnew.FromMM(width))
                    t.SetLayer(COPPER_LAYERS_1991[name])
                    t.SetNet(board.FindNet(net))
                    t.SetLocked(True)
                    board.Add(t)
    for i, net in sorted(vias.items()):
        v = pcbnew.PCB_VIA(board)
        v.SetPosition(at(holes[i]))
        v.SetLayerPair(pcbnew.F_Cu, pcbnew.B_Cu)
        v.SetWidth(pcbnew.FromMM(VIA_1991["via"]))
        v.SetDrill(pcbnew.FromMM(VIA_1991["drill"]))
        v.SetNet(board.FindNet(net))
        v.SetLocked(True)
        board.Add(v)


def board_nets(board):
    """{(ref, pad): net} as the board has them."""
    return {(fp.GetReference(), pad.GetNumber()): pad.GetNetname()
            for fp in board.GetFootprints() for pad in fp.Pads() if pad.GetNumber()}


def variant_of(name):
    """A board's name on the command line: ns4 is Э3's, else a variant's."""
    return None if name == "ns4" else name


def derive(to, frm=None):
    """The board `to` made from the board `frm`, as placed and routed: the
    pads whose nets differ put on `to`'s nets, the wires of the nets they
    left taken off (`tools/route.py ... start --from` routes those again)."""
    board = pcbnew.LoadBoard(board_path(frm))
    kinds = {ref: item.type for ref, item in load_items().items()}
    was, now = pin_nets(frm), pin_nets(to)
    left = set()
    for (ref, pin), name in sorted(now.items()):
        if was.get((ref, pin)) == name or ref not in kinds:
            continue
        pad = board.FindFootprintByReference(ref).FindPadByNumber(pad_of(kinds[ref], pin))
        net = board.FindNet(name)
        if net is None:
            net = pcbnew.NETINFO_ITEM(board, name)
            board.Add(net)
        pad.SetNet(net)
        left.add(was.get((ref, pin)))
    left.discard(None)
    for track in [t for t in board.GetTracks() if t.GetNetname() in left]:
        board.Delete(track)
    pcbnew.ZONE_FILLER(board).Fill(board.Zones())
    board.Save(board_path(to))
    return sorted(left)


def check(board, variant=None):
    """The pads' nets against the netlist: a list of differences."""
    want = {}
    kinds = {ref: item.type for ref, item in load_items().items()}
    for (ref, pin), net in pin_nets(variant).items():
        if ref in kinds:
            want[(ref, pad_of(kinds[ref], pin))] = net
    have = board_nets(board)
    out = [f"{r}.{p}: the netlist's {n}, the board's {have.get((r, p), 'no pad')}"
           for (r, p), n in sorted(want.items()) if have.get((r, p)) != n and (r, p) in have]
    out += [f"{r}.{p}: on {n} on the board, on no net in the netlist"
            for (r, p), n in sorted(have.items()) if n and (r, p) not in want]
    return out


def main(argv):
    if argv[1:2] == ["check"]:
        variant = argv[2] if len(argv) > 2 else None
        diffs = check(pcbnew.LoadBoard(board_path(variant)), variant)
        for d in diffs:
            print(d)
        print(f"{len(diffs)} differences")
        return 1 if diffs else 0
    if argv[1:2] == ["derive"] and len(argv) in (3, 4):
        to, frm = variant_of(argv[2]), variant_of(argv[3] if len(argv) == 4 else "ns4")
        left = derive(to, frm)
        print(f"{board_path(to)}: {board_path(frm)} with the wires of {', '.join(left)} taken off")
        return 0
    variant = argv[1] if len(argv) > 1 else None
    out = board_path(variant)
    board, skipped, missing, failed = build(variant)
    for s in skipped:
        print("no footprint yet:", s)
    for m in missing:
        print("no pad for", m)
    for f in failed:
        print("no place for", f)
    os.makedirs(os.path.dirname(out), exist_ok=True)
    board.Save(out)
    print(f"{len(board.GetFootprints())} footprints, {board.GetNetCount()} nets -> {out}")
    return 1 if missing or failed else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
