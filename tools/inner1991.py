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
# The factory's wire patches, the assembly drawing 3.858.420 СБ page 27 (docs/findings.md,
# "The factory wire patches restore Э3"); its note 11 lifts D84.4, D17.3 and D33.3 off
# their pads, the other two ends are soldered on with the leg in its hole.
WIRES = [("D13.1", "D84.4"), ("D17.3", "D33.3", "D47.5"), ("D15.9", "D40.9"), ("D98.3", "D70.11")]
LIFTED = ("D84.4", "D17.3", "D33.3")


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


def inner_joins(pins, group, avoid=()):
    """{net: [(pin, to pin)]}: per net, one pin of each outer group past the
    first and the first group's pin it must ring to.  pins: [(net, name, hole)];
    the pins in avoid (lifted legs) are named only when a group has no other."""
    by_net = defaultdict(lambda: defaultdict(list))
    for net, name, hole in pins:
        by_net[net][group[hole]].append(name)
    out = {}
    for net, groups in by_net.items():
        if len(groups) < 2:
            continue
        heads = sorted(sorted(g, key=lambda n: (n in avoid, _natural(n)))[0] for g in groups.values())
        heads.sort(key=_natural)
        out[net] = [(h, heads[0]) for h in heads[1:]]
    return out


def patched(pins, group, wires, lifted):
    """(pins, group) with the factory wires in: a lifted leg leaves its hole (its pad keeps
    the copper as made) for a place of its own, and each wire joins its pins' pieces as
    copper would.  wires: [(pin, pin, ...)]; lifted: pin names."""
    group = dict(group)
    fresh = max(group) + 1
    hole = {name: h for _, name, h in pins}
    for name in lifted:
        hole[name], group[fresh] = fresh, fresh
        fresh += 1
    for wire in wires:
        ids = {group[hole[name]] for name in wire}
        to = min(ids)
        group = {h: to if g in ids else g for h, g in group.items()}
    return [(net, name, hole[name]) for net, name, _ in pins], group


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
    parts = []
    for fp in board.GetFootprints():
        if fp.GetReference() in own or not [p for p in fp.Pads() if p.GetNumber()]:
            continue
        box = fp.GetBoundingBox(False)
        px = [pcbnew.ToMM(v) / pcb.MM_PER_PX for v in (box.GetLeft(), box.GetTop(), box.GetRight(), box.GetBottom())]
        one = fp.FindPadByNumber("1")
        pin1 = (pcbnew.ToMM(one.GetPosition().x) / pcb.MM_PER_PX,
                pcbnew.ToMM(one.GetPosition().y) / pcb.MM_PER_PX) if one else None
        parts.append((fp.GetReference(), px, pin1))
    return pins, holes, parts


def layers_image():
    """Mick's four layers together (NS4-all.gif), from $MS0515_DATA."""
    from PIL import Image
    data = os.environ.get("MS0515_DATA", os.path.join(ROOT, "..", "ms0515_data"))
    return Image.open(os.path.join(data, "scans", "boards", "radon17-layers", "NS4-all.gif")).convert("RGB")


def _font(size):
    from PIL import ImageFont
    try:
        return ImageFont.truetype("arialbd.ttf", size)
    except OSError:
        return ImageFont.load_default()


def draw_map(parts, path):
    """The board as Mick's layers show it, dimmed, every part boxed and
    named, pin 1 a red dot: to find the pins of the list on a board."""
    from PIL import Image, ImageDraw
    im = Image.blend(layers_image(), Image.new("RGB", (4488, 3543), (0, 0, 0)), 0.55)
    d = ImageDraw.Draw(im)
    small, big = _font(22), _font(34)
    for ref, (x0, y0, x1, y1), pin1 in parts:
        d.rectangle([x0, y0, x1, y1], outline=(230, 230, 230), width=2)
        f = big if (x1 - x0) * (y1 - y0) > 40000 else small
        w = d.textlength(ref, font=f)
        d.text(((x0 + x1 - w) / 2, (y0 + y1) / 2 - f.size / 2), ref, fill=(255, 255, 0), font=f)
        if pin1:
            d.ellipse([pin1[0] - 7, pin1[1] - 7, pin1[0] + 7, pin1[1] + 7], fill=(255, 40, 40))
    im.resize((3366, 2657)).save(path, quality=80)       # three quarters: the names still read


def _panel(image, box, size, marks):
    """A crop of image (box in its pixels) scaled to fit size, the pins of
    marks ({name: (x, y)}) ringed and named inside the frame."""
    from PIL import Image, ImageDraw
    x0, y0, x1, y1 = box
    s = min(size[0] / (x1 - x0), size[1] / (y1 - y0), 4.0)
    t = image.crop(box).resize((int((x1 - x0) * s), int((y1 - y0) * s)), Image.NEAREST)
    d = ImageDraw.Draw(t)
    f = _font(20)
    for name, (x, y) in marks.items():
        X, Y = (x - x0) * s, (y - y0) * s
        if not (0 <= X < t.size[0] and 0 <= Y < t.size[1]):
            continue
        d.ellipse([X - 16, Y - 16, X + 16, Y + 16], outline=(255, 255, 255), width=3)
        w = d.textlength(name, font=f)
        tx = X + 19 if X + 25 + w < t.size[0] else X - 22 - w
        d.rectangle([tx - 3, Y - 28, tx + w + 3, Y - 4], fill=(0, 0, 0))
        d.text((tx, Y - 28), name, fill=(255, 255, 0), font=f)
    return t


def draw_pair(a, b, where, path):
    """Two pins of the list on Mick's layers, ringed and named: one frame
    when they are near, a frame round each when they are not."""
    from PIL import Image
    image = layers_image()
    (ax, ay), (bx, by) = where[a], where[b]
    if max(abs(ax - bx), abs(ay - by)) < 500:
        box = (int(min(ax, bx)) - 140, int(min(ay, by)) - 140, int(max(ax, bx)) + 140, int(max(ay, by)) + 140)
        _panel(image, box, (900, 700), {a: where[a], b: where[b]}).save(path)
        return
    frames = [_panel(image, (int(x) - 160, int(y) - 120, int(x) + 160, int(y) + 120), (480, 360), {n: (x, y)})
              for n, (x, y) in ((a, where[a]), (b, where[b]))]
    sheet = Image.new("RGB", (frames[0].size[0] + frames[1].size[0] + 10, max(f.size[1] for f in frames)), (30, 30, 30))
    sheet.paste(frames[0], (0, 0))
    sheet.paste(frames[1], (frames[0].size[0] + 10, 0))
    sheet.save(path)


def _wires_text(wires):
    text = ["## Pairs that ring through the factory's wires", "",
            "The board as made carries white wire patches, drawn on the assembly drawing 3.858.420 СБ",
            "(page 27) and seen on every board photographed; they make the copper what Э3 draws, and",
            "Mick's traces, the copper alone, do not have them.  These pins ring through a wire you can",
            "see, not through the hidden layers, so they say nothing about layers 3..6:", "",
            "| Net | Pins joined by the wire |", "|---|---|"]
    for net, pins in wires:
        text.append(f"| `{net}` | {' - '.join(pins)} |")
    text += ["", f"Note 11 of the drawing lifts {', '.join(LIFTED)} off their pads: the leg goes to",
             "the wire only.  Ring those from the leg for the list above.  Their pads, under the bent",
             "legs, keep the copper as it was made before the patch: what a pad rings to (from the pad,",
             "not the leg) is the hidden copper the patch replaced - worth a post too.", ""]
    return text


def write(joins, shorts, planes, wires):
    text = ["# What the hidden layers join: a list to ring out", "",
            "Written by `tools/inner1991.py`; how and why: its docstring and `docs/plan.md` 8d.", "",
            "НС4 rev. 3А has eight copper layers; Mick's traces (zx-pk.ru 15146 #271) show 1, 2, 7 and 8.",
            "A net of Э3 whose pins the outer copper leaves in pieces is joined on layers 3..6.  With a",
            "board and an ohmmeter (power off), each line below is a pair that must ring: the first pin",
            "of a piece and the pin of the first piece.  Copper rings at well under an ohm; through the",
            "chips' inputs and outputs two pins read hundreds of ohms or more, which is not a ring.  A",
            "pair that does not ring is a place where the board differs from Э3 - or where our reading",
            "of Mick's traces does; either way it is worth a photo and a post.", "",
            f"`GND` and `+5V` are planes: every pin of theirs rings to every other ({planes}).", "",
            "Where the pins are: the board as Mick's layers show it, seen from the parts' side, the",
            "edge connectors (XS1, XP3, XP5, XP6) on the left; each part boxed and named, pin 1 a red",
            "dot.  A chip's pins run from pin 1 along its row and back along the other row.", "",
            "![The board, every part named](inner-layers/board-map.jpg)", ""]
    text += ["## Pairs that must ring", "", "| Net | Pin | Rings to |", "|---|---|---|"]
    for net in sorted(joins):
        for a, b in joins[net]:
            text.append(f"| `{net}` | {a} | {b} |")
    text += [""] + _wires_text(wires)
    text += ["## Pairs that must not ring", "",
             "The outer copper as traced joins these pins of two nets of Э3: a trace error, a factory",
             "patch (a cut and a wire), or the board's own.  On a board built to Э3 they are apart.", "",
             "| Net | Pin | Net | Pin |", "|---|---|---|---|"]
    for a, pa, b, pb in shorts:
        text.append(f"| `{a}` | {pa} | `{b}` | {pb} |")
    text += ["", "Each of them on Mick's layers (1 green, 2 orange, 7 brown, 8 blue):", ""]
    for k, (a, pa, b, pb) in enumerate(shorts, 1):
        text += [f"{k}. {pa} (`{a}`) and {pb} (`{b}`)", "",
                 f"   ![{pa} and {pb}](inner-layers/apart-{k}.png)", ""]
    with open(OUT, "w", encoding="utf-8", newline="\n") as f:
        f.write("\n".join(text) + "\n")


def main():
    pins, holes, parts = board_pins()
    lines = json.load(open(os.path.join(ROOT, "netlist", "lines-1991.json"), encoding="utf-8"))
    group = outer_groups(lines, len(holes))
    planes = ", ".join(f"{sum(1 for p in pins if p[0] == pl)} `{pl}` pins" for pl in PLANES)
    legs, by_legs = patched(pins, group, [], LIFTED)
    wired, by_wires = patched(pins, group, WIRES, LIFTED)
    signal = [p for p in wired if p[0] not in PLANES]
    joins, shorts = inner_joins(signal, by_wires, LIFTED),outer_shorts(legs, by_legs)
    net_of = {name: net for net, name, _ in pins}
    write(joins, shorts, planes, [(net_of[w[0]], w) for w in WIRES])
    pictures = os.path.join(os.path.dirname(OUT), "inner-layers")
    os.makedirs(pictures, exist_ok=True)
    draw_map(parts, os.path.join(pictures, "board-map.jpg"))
    where = {name: holes[h] for _, name, h in pins}
    for k, (_, pa, _, pb) in enumerate(shorts, 1):
        draw_pair(pa, pb, where, os.path.join(pictures, f"apart-{k}.png"))
    print(f"{len(joins)} nets joined inside, {sum(len(v) for v in joins.values())} pairs; "
          f"{len(shorts)} outer joins of two nets -> {OUT}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
