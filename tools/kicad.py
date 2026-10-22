#!/usr/bin/env python3
"""The НС4 schematic for KiCad, generated from the netlist.

    python tools/kicad.py          - write kicad/ns4*.kicad_sch
    python tools/kicad.py check    - the same without writing (exit 1 when
                                     the committed files are stale)

For whoever builds the board in copper: the drawing is electrically the
netlist (netlist/parts.csv, pinouts.csv, nets.csv - Э3 as read), the one
the board model is generated from and has run the machine's software on.
A root sheet holds one sheet per Э3 sheet; every part goes onto the sheet
most of its pins were read from (parts on power only: sheet 1, the power
table's).  A part is a box with its real pin numbers and names; every pin
ends in a global label named after its net, power pins in power symbols,
pins on no net in a no-connect flag - so the sheets join through their
labels as Э3's do through its cross-references.  The layout is a plain
grid, to be tidied by hand; the connections are what the file is for.
"""
import os
import shutil
import subprocess
import sys
import uuid
import xml.etree.ElementTree as ET
from collections import Counter, defaultdict
from dataclasses import dataclass, field

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import netlist as nl  # noqa: E402

REPO = os.path.join(HERE, "..")
PITCH = 2.54
BOX_W = 22.86
COL_W = BOX_W + 30 * PITCH          # a box, its labels on the left, a gap
COL_H = 300.0                       # mm of boxes in a column before the next one
POWER_NETS = ("GND", "+5V", "+12V", "-12V", "-5V")
WIDE = 24                           # pins from which a DIP is 15 mm wide (footprints.py)


@dataclass
class Pin:
    number: int
    name: str
    net: str        # "" for a pin on no net


@dataclass
class Item:
    ref: str
    value: str
    footprint: str
    sheet: str
    pins: list = field(default_factory=list)


class Ids:
    """UUIDs from a counter: the same netlist gives the same files."""

    def __init__(self, seed):
        self.ns, self.n = uuid.uuid5(uuid.NAMESPACE_URL, seed), 0
        self.pwr = 0                # power symbols' references, across the sheets

    def __call__(self):
        self.n += 1
        return str(uuid.uuid5(self.ns, str(self.n)))


def esc(s):
    return s.replace("\\", "\\\\").replace('"', '\\"')


def footprint(part):
    if part.ref.startswith("D") and part.pins >= 8 and part.pins % 2 == 0:
        width = "15.24" if part.pins >= WIDE else "7.62"
        return f"Package_DIP:DIP-{part.pins}_W{width}mm"
    return ""


def items(parts, pinouts, conns):
    """Every part with its pins, nets and sheet."""
    net_of, name_of, sheets = {}, {}, defaultdict(Counter)
    for c in conns:
        net_of[(c.ref, c.pin)] = c.net
        if c.pin_name:
            name_of[(c.ref, c.pin)] = c.pin_name
        sheets[c.ref][c.sheet] += 1
    out = []
    for ref, part in parts.items():
        names = pinouts.get(part.type, {})
        pins = []
        for n in range(1, part.pins + 1):
            net = part.power.get(n) or net_of.get((ref, n), "")
            pins.append(Pin(n, names.get(n) or name_of.get((ref, n), ""), net))
        sheet = sheets[ref].most_common(1)[0][0] if sheets[ref] else "1"
        value = part.type + (f" {part.value}" if part.value else "")
        out.append(Item(ref, value, footprint(part), sheet, pins))
    return out


def sort_key(ref):
    digits = "".join(ch for ch in ref if ch.isdigit())
    return ref.rstrip("0123456789"), int(digits or 0), ref


def box_h(item):
    return (len(item.pins) + 1) * PITCH


def symbol_def(item):
    """A box with every pin on its left, one a row."""
    h = box_h(item)
    lines = [f'    (symbol "NS4:{item.ref}" (pin_names (offset 1.016)) (exclude_from_sim no) (in_bom yes) (on_board yes)',
             f'      (property "Reference" "{item.ref}" (at 0 {h / 2 + 1.27:.2f} 0) (effects (font (size 1.27 1.27))))',
             f'      (property "Value" "{esc(item.value)}" (at 0 {-h / 2 - 1.27:.2f} 0) (effects (font (size 1.27 1.27))))',
             f'      (property "Footprint" "{item.footprint}" (at 0 0 0) (effects (font (size 1.27 1.27)) (hide yes)))',
             '      (property "Datasheet" "" (at 0 0 0) (effects (font (size 1.27 1.27)) (hide yes)))',
             f'      (symbol "{item.ref}_0_1"',
             f'        (rectangle (start 0 {h / 2:.2f}) (end {BOX_W} {-h / 2:.2f}) (stroke (width 0.254) (type default)) (fill (type background)))',
             '      )',
             f'      (symbol "{item.ref}_1_1"']
    for i, pin in enumerate(item.pins):
        y = h / 2 - (i + 1) * PITCH
        lines.append(f'        (pin passive line (at -2.54 {y:.2f} 0) (length 2.54) (name "{esc(pin.name or "~")}" '
                     f'(effects (font (size 1.0 1.0)))) (number "{pin.number}" (effects (font (size 1.0 1.0)))))')
    lines += ['      )', '    )']
    return "\n".join(lines)


def power_def(net):
    """A power symbol: its pin's name is the net it joins."""
    return "\n".join([
        f'    (symbol "NS4:{net}" (power) (pin_names (offset 0)) (exclude_from_sim no) (in_bom yes) (on_board yes)',
        '      (property "Reference" "#PWR" (at 0 -3.81 0) (effects (font (size 1.27 1.27)) (hide yes)))',
        f'      (property "Value" "{net}" (at 0 3.81 0) (effects (font (size 1.0 1.0))))',
        '      (property "Footprint" "" (at 0 0 0) (effects (font (size 1.27 1.27)) (hide yes)))',
        '      (property "Datasheet" "" (at 0 0 0) (effects (font (size 1.27 1.27)) (hide yes)))',
        f'      (symbol "{net}_0_1"',
        '        (polyline (pts (xy -1.27 0) (xy 1.27 0)) (stroke (width 0) (type default)) (fill (type none)))',
        '      )',
        f'      (symbol "{net}_1_1"',
        f'        (pin power_in line (at 0 0 90) (length 0) (name "{net}" (effects (font (size 1.27 1.27)))) '
        '(number "1" (effects (font (size 1.27 1.27)))))',
        '      )', '    )'])


class Sheet:
    """One sheet's text: the instances placed on a grid, their pins' ends."""

    def __init__(self, ids, path, project):
        self.ids, self.path, self.project = ids, path, project
        self.body, self.width, self.height = [], 0.0, 0.0

    def place(self, item, x, y):
        h = box_h(item)
        self.body += [f'  (symbol (lib_id "NS4:{item.ref}") (at {x:.2f} {y + h / 2:.2f} 0) (unit 1) (exclude_from_sim no) (in_bom yes) (on_board yes) (dnp no) (uuid "{self.ids()}")',
                      f'    (property "Reference" "{item.ref}" (at {x + 2.54:.2f} {y - 1.27:.2f} 0) (effects (font (size 1.27 1.27)) (justify left)))',
                      f'    (property "Value" "{esc(item.value)}" (at {x + 2.54:.2f} {y + h + 1.27:.2f} 0) (effects (font (size 1.27 1.27)) (justify left)))',
                      f'    (property "Footprint" "{item.footprint}" (at {x:.2f} {y:.2f} 0) (effects (font (size 1.27 1.27)) (hide yes)))',
                      f'    (property "Datasheet" "" (at {x:.2f} {y:.2f} 0) (effects (font (size 1.27 1.27)) (hide yes)))']
        self.body += [f'    (pin "{p.number}" (uuid "{self.ids()}"))' for p in item.pins]
        self.body += [f'    (instances (project "{self.project}" (path "{self.path}" (reference "{item.ref}") (unit 1))))', '  )']
        for i, pin in enumerate(item.pins):
            self.end(pin, x - PITCH, y + (i + 1) * PITCH)
        self.width = max(self.width, x + BOX_W)
        self.height = max(self.height, y + h)

    def end(self, pin, tx, py):
        """What a pin's tip at (tx, py) ends in."""
        if not pin.net:
            self.body.append(f'  (no_connect (at {tx:.2f} {py:.2f}) (uuid "{self.ids()}"))')
            return
        px = tx - 2 * PITCH
        self.body.append(f'  (wire (pts (xy {px:.2f} {py:.2f}) (xy {tx:.2f} {py:.2f})) (stroke (width 0) (type default)) (uuid "{self.ids()}"))')
        if pin.net in POWER_NETS:
            self.ids.pwr += 1
            ref = f"#PWR{self.ids.pwr:04d}"
            self.body += [f'  (symbol (lib_id "NS4:{pin.net}") (at {px:.2f} {py:.2f} 90) (unit 1) (exclude_from_sim no) (in_bom yes) (on_board yes) (dnp no) (uuid "{self.ids()}")',
                          f'    (property "Reference" "{ref}" (at {px:.2f} {py:.2f} 0) (effects (font (size 1.27 1.27)) (hide yes)))',
                          f'    (property "Value" "{pin.net}" (at {px - 3.81:.2f} {py:.2f} 0) (effects (font (size 1.0 1.0)) (justify right)))',
                          f'    (property "Footprint" "" (at {px:.2f} {py:.2f} 0) (effects (font (size 1.27 1.27)) (hide yes)))',
                          f'    (property "Datasheet" "" (at {px:.2f} {py:.2f} 0) (effects (font (size 1.27 1.27)) (hide yes)))',
                          f'    (pin "1" (uuid "{self.ids()}"))',
                          f'    (instances (project "{self.project}" (path "{self.path}" (reference "{ref}") (unit 1))))', '  )']
        else:
            self.body.append(f'  (global_label "{esc(pin.net)}" (shape passive) (at {px:.2f} {py:.2f} 180) (fields_autoplaced yes) '
                             f'(effects (font (size 1.0 1.0)) (justify right)) (uuid "{self.ids()}"))')


def layout(sheet, members):
    """The parts of a sheet in columns, by reference."""
    x, y = 30 * PITCH, 10 * PITCH
    for item in members:
        if y + box_h(item) > COL_H and y > 10 * PITCH:
            x, y = x + COL_W, 10 * PITCH
        sheet.place(item, x, y)
        y += box_h(item) + 4 * PITCH


def sheet_text(ids, members, title, own_uuid, path, project):
    sheet = Sheet(ids, path, project)
    layout(sheet, members)
    nets = {p.net for m in members for p in m.pins if p.net in POWER_NETS}
    lib = [symbol_def(m) for m in members] + [power_def(n) for n in sorted(nets)]
    return "\n".join([
        '(kicad_sch (version 20250114) (generator "ms0515_fpga") (generator_version "10.0")',
        f'  (uuid "{own_uuid}")',
        f'  (paper "User" {round(sheet.width + 40)} {round(sheet.height + 40)})',
        f'  (title_block (title "{esc(title)}") (company "ms0515-fpga") '
        '(comment 1 "generated from netlist/ by tools/kicad.py - do not edit by hand"))',
        '  (lib_symbols', *lib, '  )', *sheet.body, ')', ''])


def root_text(ids, root_uuid, sheets, project):
    body, y = [], 20.0
    for number, (sheet_uuid, file) in sorted(sheets.items(), key=lambda s: int(s[0])):
        body += [f'  (sheet (at 20 {y:.2f}) (size 60 10) (fields_autoplaced yes) (stroke (width 0.1524) (type solid)) '
                 f'(fill (color 0 0 0 0.0000)) (uuid "{sheet_uuid}")',
                 f'    (property "Sheetname" "Э3 sheet {number}" (at 20 {y - 0.7:.2f} 0) (effects (font (size 1.27 1.27)) (justify left bottom)))',
                 f'    (property "Sheetfile" "{file}" (at 20 {y + 10.6:.2f} 0) (effects (font (size 1.27 1.27)) (justify left top)))',
                 f'    (instances (project "{project}" (path "/{root_uuid}" (page "{int(number) + 1}"))))', '  )']
        y += 20
    return "\n".join([
        '(kicad_sch (version 20250114) (generator "ms0515_fpga") (generator_version "10.0")',
        f'  (uuid "{root_uuid}")', '  (paper "A4")',
        '  (title_block (title "НС4 (3.858.420) - the system module of the МС 0515") (company "ms0515-fpga") '
        '(comment 1 "generated from netlist/ by tools/kicad.py - do not edit by hand"))',
        '  (lib_symbols)', *body, '  (sheet_instances (path "/" (page "1")))', ')', ''])


def generate(parts, pinouts, conns, project="ns4"):
    """{file name: text} of the root sheet and one sheet per Э3 sheet."""
    ids = Ids(project)
    all_items = sorted(items(parts, pinouts, conns), key=lambda i: sort_key(i.ref))
    by_sheet = defaultdict(list)
    for item in all_items:
        by_sheet[item.sheet].append(item)
    root_uuid = ids()
    sheets = {n: (ids(), f"{project}-sheet{n}.kicad_sch") for n in by_sheet}
    files = {f"{project}.kicad_sch": root_text(ids, root_uuid, sheets, project)}
    for number, members in sorted(by_sheet.items(), key=lambda s: int(s[0])):
        sheet_uuid, file = sheets[number]
        files[file] = sheet_text(ids, members, f"НС4 - Э3 sheet {number}", sheet_uuid,
                                 f"/{root_uuid}/{sheet_uuid}", project)
    files[f"{project}.kicad_pro"] = '{\n  "meta": {"filename": "%s.kicad_pro", "version": 1}\n}\n' % project
    return files


def kicad_cli():
    """KiCad's command line: $KICAD_CLI, else kicad-cli on the PATH; None without."""
    return os.environ.get("KICAD_CLI") or shutil.which("kicad-cli")


def nets_of(parts, conns):
    """{net: {(ref, pin)}} - the netlist with the parts' power pins."""
    nets = defaultdict(set)
    for c in conns:
        nets[c.net].add((c.ref, str(c.pin)))
    for ref, part in parts.items():
        for pin, net in part.power.items():
            nets[net].add((ref, str(pin)))
    return nets


def exported_nets(folder, project="ns4"):
    """The nets KiCad itself reads out of the written sheets (kicad-cli's
    XML netlist), power symbols left out; None without kicad-cli."""
    cli = kicad_cli()
    if not cli:
        return None
    xml = os.path.join(folder, project + ".xml")
    subprocess.run([cli, "sch", "export", "netlist", "--format", "kicadxml", "-o", xml,
                    os.path.join(folder, project + ".kicad_sch")], check=True, capture_output=True)
    nets = {}
    for net in ET.parse(xml).getroot().iter("net"):
        nodes = {(n.get("ref"), n.get("pin")) for n in net.iter("node") if not n.get("ref").startswith("#")}
        if nodes and not net.get("name").startswith("unconnected"):
            nets[net.get("name").lstrip("/")] = nodes
    return nets


def differences(ours, theirs):
    """The nets, as sets of pins, one side has and the other has not."""
    a = {frozenset(v): k for k, v in ours.items()}
    b = {frozenset(v): k for k, v in theirs.items()}
    return sorted(a[s] for s in a if s not in b), sorted(b[s] for s in b if s not in a)


def ns4_files():
    n = os.path.join(REPO, "netlist")
    return generate(*nl.load(os.path.join(n, "parts.csv"), os.path.join(n, "pinouts.csv"),
                             os.path.join(n, "nets.csv")))


def main(argv):
    out = os.path.join(REPO, "kicad")
    files = ns4_files()
    if len(argv) > 1 and argv[1] == "check":
        stale = [f for f, text in files.items()
                 if not os.path.exists(os.path.join(out, f))
                 or open(os.path.join(out, f), encoding="utf-8").read() != text]
        for f in stale:
            print(f"stale: kicad/{f}")
        return 1 if stale else 0
    os.makedirs(out, exist_ok=True)
    for f, text in files.items():
        with open(os.path.join(out, f), "w", encoding="utf-8", newline="\n") as fh:
            fh.write(text)
    print(f"kicad/: {len(files)} files")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
