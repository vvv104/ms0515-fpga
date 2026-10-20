#!/usr/bin/env python3
"""Read a netlist out of Mick's vector PDF of the Э3 redraw.

    python tools/mick_pdf.py [PDF]

PDF  mc-0515.pdf of Mick's P-CAD redraw (default:
     $MS0515_DATA/scans/anasana-2026-10-20/pcad/mc-0515.pdf, MS0515_DATA
     falling back to ../ms0515_data beside the repository)

Writes netlist/mick.csv (net,ref,pin,page) in the netlist's own naming:
DDn -> Dn, net labels "ИМЯ_Н"/"ИМЯ_L" -> "ИМЯ H"/"ИМЯ L" without the
spaces Mick dropped (the comparison ignores spaces).

How the drawing is read (P-CAD 2002's PDF output):
- line width 0.06 is a wire or a pin stub, 0.12 a symbol body, an
  inversion circle or a ground bar, 0.96 a bus (only a picture: buses
  join nothing, the labels on the wires do);
- thin segments that share an end, or whose end lies on another one,
  are one net; a filled dot is a junction;
- a pin is a thin segment with one end on a body; its number is the
  digit word nearest to the segment, its part the body it touches, named
  by the nearest DD/R/C/VD/... word;
- a net label is a word lying on a thin segment (just above it); a thin
  end on a ground bar puts the net on GND; "+5V" and the like by text.
"""
import csv
import os
import re
import sys
from collections import defaultdict

import fitz

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DATA = os.environ.get("MS0515_DATA", os.path.join(ROOT, "..", "ms0515_data"))
DEFAULT = os.path.join(DATA, "scans", "anasana-2026-10-20", "pcad", "mc-0515.pdf")
TOL = 0.12
REF = re.compile(r"(DD|R|C|VD|VT|DT|HP|BQ|E|XP|XS|SA)(\d+)(?::(\d+))?$")
POWER = {"+5V": "+5V", "+12V": "+12V", "-12V": "-12V", "+5В": "+5V"}


class Union:
    def __init__(self):
        self.parent = {}

    def find(self, a):
        self.parent.setdefault(a, a)
        while self.parent[a] != a:
            self.parent[a] = self.parent[self.parent[a]]
            a = self.parent[a]
        return a

    def join(self, a, b):
        self.parent[self.find(a)] = self.find(b)


def on_segment(p, a, b, tol=TOL):
    ax, ay, bx, by, px, py = a.x, a.y, b.x, b.y, p.x, p.y
    dx, dy = bx - ax, by - ay
    n = dx * dx + dy * dy
    if n == 0:
        return abs(px - ax) + abs(py - ay) < tol
    t = max(0.0, min(1.0, ((px - ax) * dx + (py - ay) * dy) / n))
    return abs(px - (ax + t * dx)) < tol and abs(py - (ay + t * dy)) < tol


def primitives(page):
    """Thin wires and stubs, body lines, junction dots.  The slanted bus
    entries are dropped: two of them often meet on the bus line at one
    point (a wire crossing the bus to a renamed net), and a bus joins
    nothing - only the net labels on the wires do."""
    thin, body, dots, bus = [], [], [], []
    for g in page.get_drawings():
        w = round(g.get("width") or 0, 2)
        if g.get("fill") is not None and g["rect"].width < 2:
            dots.append(g["rect"])
            continue
        for it in g["items"]:
            if it[0] == "l":
                (thin if w <= 0.07 else body if w < 0.5 else bus).append((it[1], it[2]))
            elif it[0] == "c" and w < 0.5:
                body.append((it[1], it[4]))

    def entry(a, b):
        slanted = abs(a.x - b.x) > 0.05 and abs(a.y - b.y) > 0.05
        return slanted and any(on_segment(p, c, d, 0.3) for p in (a, b) for c, d in bus)

    return [s for s in thin if not entry(*s)], body, dots


def nets_of(thin):
    u = Union()
    for i, (a, b) in enumerate(thin):
        u.find(i)
        for j in range(i):
            c, d = thin[j]
            if any(on_segment(p, c, d) for p in (a, b)) or any(on_segment(p, a, b) for p in (c, d)):
                u.join(i, j)
    return u


def nearest(words, pt, pattern, limit):
    best = None
    for w in words:
        if not pattern.fullmatch(w[4]):
            continue
        cx, cy = (w[0] + w[2]) / 2, (w[1] + w[3]) / 2
        dist = abs(cx - pt[0]) + abs(cy - pt[1])
        if dist < limit and (best is None or dist < best[0]):
            best = (dist, w[4])
    return best[1] if best else None


def pin_number(words, a, b, digits):
    """P-CAD writes a pin's number beside its stub on the lower-coordinate
    side: left of a vertical stub, above a horizontal one (in the page's
    unrotated space), within the stub's length."""
    vertical = abs(a.x - b.x) < 0.01
    lo, hi = (min(a.y, b.y), max(a.y, b.y)) if vertical else (min(a.x, b.x), max(a.x, b.x))
    line = a.x if vertical else a.y
    best = None
    for w in words:
        if not digits.fullmatch(w[4]):
            continue
        cx, cy = (w[0] + w[2]) / 2, (w[1] + w[3]) / 2
        along, across = (cy, line - cx) if vertical else (cx, line - cy)
        if lo - 0.5 <= along <= hi + 0.5 and 0 < across < 4.5:
            if best is None or across < best[0]:
                best = (across, w[4])
    return best[1] if best else None


def ref_of(words, box, limit=12.0):
    """The reference word nearest to a body's box (0 inside it)."""
    best = None
    for w in words:
        if not REF.match(w[4]):
            continue
        cx, cy = (w[0] + w[2]) / 2, (w[1] + w[3]) / 2
        dx = max(box[0] - cx, 0, cx - box[2])
        dy = max(box[1] - cy, 0, cy - box[3])
        d = dx + dy
        if d < limit and (best is None or d < best[0]):
            best = (d, w[4])
    return best[1] if best else None


def body_groups(body):
    u = Union()
    for i, (a, b) in enumerate(body):
        u.find(i)
        for j in range(i):
            c, d = body[j]
            if any(on_segment(p, c, d, 0.05) for p in (a, b)) or \
                    any(on_segment(p, a, b, 0.05) for p in (c, d)):
                u.join(i, j)
    groups = defaultdict(list)
    for i in range(len(body)):
        groups[u.find(i)].append(i)
    return u, groups


def read_page(page, pno):
    thin, body, dots = primitives(page)
    words = page.get_text("words")
    u = nets_of(thin)
    bu, groups = body_groups(body)
    box = {}
    for g, members in groups.items():
        xs = [p.x for i in members for p in body[i]]
        ys = [p.y for i in members for p in body[i]]
        box[g] = (min(xs), min(ys), max(xs), max(ys))
    refs = {g: ref_of(words, b) for g, b in box.items()}
    pins, labels, ground = [], defaultdict(set), set()
    digits = re.compile(r"\d{1,2}")
    for i, (a, b) in enumerate(thin):
        for inner, outer in ((a, b), (b, a)):
            hit = [j for j, (c, d) in enumerate(body) if on_segment(inner, c, d, 0.08)]
            if not hit:
                continue
            g = bu.find(hit[0])
            num = pin_number(words, a, b, digits)
            if refs.get(g) and num:
                pins.append((u.find(i), refs[g], int(num)))
            break
        for j, (c, d) in enumerate(body):
            if abs(c.y - d.y) < 0.01 and any(on_segment(p, c, d, 0.05) for p in (a, b)) \
                    and not any(on_segment(a, *body[k], 0.05) and on_segment(b, *body[k], 0.05)
                                for k in [j]):
                pass
    for w in words:
        name = w[4]
        if not re.fullmatch(r"[А-ЯA-Z0-9]+_[НLH]|\+5V|\+12V|-12V", name):
            continue
        cx, cy = (w[0] + w[2]) / 2, (w[1] + w[3]) / 2
        if w[3] - w[1] > w[2] - w[0]:            # turned text: the wire runs beside it
            feet = [fitz.Point(w[2] + 0.6, cy), fitz.Point(w[0] - 0.6, cy)]
        else:
            feet = [fitz.Point(cx, w[3] + 0.6), fitz.Point(cx, w[1] - 0.6)]
        hit = None
        for foot in feet:
            for i, (a, b) in enumerate(thin):
                if on_segment(foot, a, b, 1.0):
                    hit = i
                    break
            if hit is not None:
                break
        if hit is not None:
            labels[u.find(hit)].add(POWER.get(name, name))
    return pins, labels, pno


def normal(name):
    if name in POWER.values() or name == "GND":
        return name
    base, pol = name[:-2], name[-1]
    return f"{base} {'H' if pol in 'НH' else 'L'}"


def main():
    pdf = sys.argv[1] if len(sys.argv) > 1 else DEFAULT
    doc = fitz.open(pdf)
    rows = []
    for pno, page in enumerate(doc, 1):
        pins, labels, _ = read_page(page, pno)
        for net, ref, pin in pins:
            names = sorted(labels.get(net, ()))
            name = normal(names[0]) if names else f"~p{pno}n{net}"
            m = REF.match(ref)
            r = ("D" if m.group(1) == "DD" else m.group(1)) + m.group(2)
            rows.append((name, r, pin, pno, "|".join(names[1:])))
    with open(os.path.join(ROOT, "netlist", "mick.csv"), "w", encoding="utf-8", newline="") as f:
        w = csv.writer(f, lineterminator="\n")
        w.writerow(["net", "ref", "pin", "page", "other_labels"])
        w.writerows(sorted(set(rows)))
    print(len(rows), "pins read,", len({r[0] for r in rows}), "nets")


if __name__ == "__main__":
    main()
