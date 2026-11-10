#!/usr/bin/env python3
"""The footprints of the 1991 board's own parts, for kicad/ns4.pretty (docs/plan.md stage 8d).

    python tools/pretty.py          - write them
    python tools/pretty.py check    - exit 1 when one is stale

The parts stand where the 1991 board had them (netlist/placement-1991.csv), so
their footprints have the 1991 board's holes: the К10-17s 5.08 mm apart, the
С1-4 resistors, the diodes and the wire links 12.7 mm, the К53-14s 25.4 mm,
the КМ-6Б 10.16 mm, the crystals 5.08 and 12.7 mm, the trimmers СП5-16ВА as a
corner (the wiper 2.54 mm beside pin 1, pin 2 3.175 mm from the wiper), and the
D-subs ОНП-ЖГ-79 hole for hole as the board has them (their pitch is not the
imported D-sub's).  Pads 1.5 mm, the 1991 board's rings being 1.42..1.48 mm
(Mick's traces).  DIN-5_Socket_Horizontal is drawn by hand and left alone.
"""
import csv
import os
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
PRETTY = os.path.join(ROOT, "kicad", "ns4.pretty")
PLACEMENT = os.path.join(ROOT, "netlist", "placement-1991.csv")
MM_PER_PX = 2.54 / 39.4
PAD, DRILL, PIN_DRILL = 1.5, 0.8, 1.0

# name: (pitch, body (w, h) or None, pin 1 square, descr)
TWO_PIN = {
    "R_Axial_P12.70mm": (12.7, (6.5, 2.5), False, "С1-4 / С2-23 resistor, 12.7 mm, as on the 1991 board"),
    "C_Disc_P5.08mm": (5.08, (5.0, 2.5), False, "К10-17 capacitor, 5.08 mm, as on the 1991 board"),
    "Wire_Link_P12.70mm": (12.7, None, False, "wire link E1..E8, 12.7 mm, as on the 1991 board"),
    "CP_Axial_P25.40mm": (25.4, (16.0, 7.0), True, "К53-14 tantalum, axial, 25.4 mm, pin 1 the +"),
    "C_Rect_P10.16mm": (10.16, (12.0, 4.5), False, "КМ-6Б capacitor, 10.16 mm, as on the 1991 board"),
    "Crystal_RK169_P5.08mm": (5.08, (11.5, 4.7), False, "РК 169МА crystal, 5.08 mm, as on the 1991 board"),
    "Crystal_RK353_P12.70mm": (12.7, (20.0, 8.0), False, "РК 353БВ crystal, 12.7 mm, as on the 1991 board"),
}
DSUBS = {"DSUB-9_ONP-ZhG-79": "XP5", "DSUB-15_ONP-ZhG-79": "XP6", "DSUB-25_ONP-ZhG-79": "XP3"}


def pad(name, x, y, square=False, drill=DRILL):
    shape = "rect" if square else "circle"
    return (f'\t(pad "{name}" thru_hole {shape} (at {x:.3f} {y:.3f}) (size {PAD} {PAD}) (drill {drill})'
            f' (layers "*.Cu" "*.Mask"))')


def footprint(name, descr, pads, box):
    """A footprint's text: its pads and a silkscreen box (x0, y0, x1, y1)."""
    x0, y0, x1, y1 = box
    lines = [f'(footprint "{name}"', "\t(version 20241229)", '\t(generator "pretty.py")', '\t(layer "F.Cu")',
             f'\t(descr "{descr}")',
             f'\t(property "Reference" "REF**" (at {x0:.3f} {y0 - 1.5:.3f} 0) (layer "F.SilkS")'
             ' (effects (font (size 1 1) (thickness 0.15))))',
             f'\t(property "Value" "{name}" (at {x0:.3f} {y1 + 1.5:.3f} 0) (layer "F.Fab")'
             ' (effects (font (size 1 1) (thickness 0.15))))',
             "\t(attr through_hole)",
             f'\t(fp_rect (start {x0:.3f} {y0:.3f}) (end {x1:.3f} {y1:.3f})'
             ' (stroke (width 0.12) (type solid)) (fill no) (layer "F.SilkS"))']
    lines += pads
    lines.append(")")
    return "\n".join(lines) + "\n"


def two_pin(name):
    pitch, body, square, descr = TWO_PIN[name]
    w, h = body or (pitch - PAD - 0.4, 0.6)
    box = ((pitch - w) / 2, -h / 2, (pitch + w) / 2, h / 2)
    return footprint(name, descr, [pad("1", 0, 0, square), pad("2", pitch, 0)], box)


def trimmer():
    pads = [pad("1", -2.54, 0), pad("2", 0, -3.175), pad("3", 0, 0)]
    return footprint("Trimmer_SP5-16VA", "СП5-16ВА trimmer as on the 1991 board: pin 3 (the wiper) 2.54 mm "
                     "beside pin 1, pin 2 3.175 mm from pin 3", pads, (-4.5, -5.5, 2.0, 1.5))


def dsub(name, ref):
    """A D-sub's pads as the 1991 board's holes stand, pin 1 at the origin."""
    at = {}
    with open(PLACEMENT, encoding="utf-8") as f:
        for row in csv.DictReader(line for line in f if not line.startswith("#")):
            if row["ref"] == ref:
                at[row["pad"]] = (float(row["x"]) * MM_PER_PX, float(row["y"]) * MM_PER_PX)
    x1, y1 = at["1"]
    rel = {p: (round(x - x1, 2), round(y - y1, 2)) for p, (x, y) in at.items()}
    xs, ys = [v[0] for v in rel.values()], [v[1] for v in rel.values()]
    pads = [pad(p, *rel[p], square=p == "1", drill=PIN_DRILL) for p in sorted(rel, key=int)]
    return footprint(name, f"ОНП-ЖГ-79 D-sub ({ref} of НС4) hole for hole as the 1991 board has it", pads,
                     (min(xs) - 2, min(ys) - 2, max(xs) + 2, max(ys) + 2))


def hole(drill):
    """A bare hole of the 1991 board (its mounting holes): no copper, no
    courtyard - a lug's pad may come near it, as on the 1991 board."""
    name = f"Hole_{drill}mm_NPTH"
    r = drill / 2
    lines = [f'(footprint "{name}"', "\t(version 20241229)", '\t(generator "pretty.py")', '\t(layer "F.Cu")',
             f'\t(descr "a {drill} mm hole of the 1991 board, no copper")',
             f'\t(property "Reference" "REF**" (at 0 {-r - 1.5:.3f} 0) (layer "F.SilkS") (hide yes)'
             ' (effects (font (size 1 1) (thickness 0.15))))',
             f'\t(property "Value" "{name}" (at 0 {r + 1.5:.3f} 0) (layer "F.Fab")'
             ' (effects (font (size 1 1) (thickness 0.15))))',
             '\t(attr exclude_from_pos_files exclude_from_bom)',
             f'\t(fp_circle (center 0 0) (end {r:.3f} 0) (stroke (width 0.1) (type solid)) (fill no) (layer "F.Fab"))',
             f'\t(pad "" np_thru_hole circle (at 0 0) (size {drill} {drill}) (drill {drill}) (layers "*.Cu" "*.Mask"))',
             ")"]
    return name, "\n".join(lines) + "\n"


def footprints():
    """{name: text} of every footprint this writes."""
    made = {name: two_pin(name) for name in TWO_PIN}
    made["Trimmer_SP5-16VA"] = trimmer()
    made.update({name: dsub(name, ref) for name, ref in DSUBS.items()})
    made.update(dict(hole(d) for d in (4.8, 3.2)))
    return made


def stale():
    out = []
    for name, text in footprints().items():
        path = os.path.join(PRETTY, name + ".kicad_mod")
        if not os.path.exists(path) or open(path, encoding="utf-8").read() != text:
            out.append(name)
    return out


def main(argv):
    if len(argv) > 1 and argv[1] == "check":
        bad = stale()
        for name in bad:
            print(f"error: kicad/ns4.pretty/{name}.kicad_mod is stale")
        return 1 if bad else 0
    for name, text in footprints().items():
        with open(os.path.join(PRETTY, name + ".kicad_mod"), "w", encoding="utf-8", newline="\n") as f:
            f.write(text)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
