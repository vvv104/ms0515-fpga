#!/usr/bin/env python3
"""The НС4 netlist: load netlist/*.csv and check it.

    python tools/netlist.py check   - list the problems (exit 1 on errors)
    python tools/netlist.py stats   - parts, nets and pins read so far

netlist/parts.csv    ref,type,value,pins,gnd,vcc,vcc12,source,note
                     one row per part; gnd/vcc/vcc12 are the power pins
                     ("2,6" for several) as the power table of sheet 1 gives
                     them - those pins are on the power nets GND / +5V / +12V
                     and are not drawn on the sheets
netlist/pinouts.csv  type,pin,name,source - the standard pinout of a type
netlist/nets.csv     net,ref,pin,pin_name,sheet,note - one row per pin on a
                     net, with the pin name as drawn and the sheet it was
                     read from; a note containing "open" marks a net known
                     to end on this board (a connector to read, a test point)

Lines starting with '#' are comments.
"""
import csv
import os
import sys
from collections import defaultdict
from dataclasses import dataclass, field

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "netlist")
POWER_COLUMNS = (("gnd", "GND"), ("vcc", "+5V"), ("vcc12", "+12V"))


@dataclass
class Part:
    ref: str
    type: str
    value: str
    pins: int
    power: dict = field(default_factory=dict)


@dataclass
class Conn:
    net: str
    ref: str
    pin: int
    pin_name: str
    sheet: str
    note: str


@dataclass
class Problem:
    error: bool
    text: str


def rows(path):
    with open(path, encoding="utf-8", newline="") as f:
        lines = [line for line in f if not line.startswith("#")]
    return list(csv.DictReader(lines))


def load_parts(path):
    parts = {}
    for r in rows(path):
        power = {}
        for column, net in POWER_COLUMNS:
            for pin in filter(None, (r[column] or "").split(",")):
                power[int(pin)] = net
        parts[r["ref"]] = Part(r["ref"], r["type"], r["value"], int(r["pins"]), power)
    return parts


def load_pinouts(path):
    pinouts = defaultdict(dict)
    for r in rows(path):
        pinouts[r["type"]][int(r["pin"])] = r["name"]
    return dict(pinouts)


def load_nets(path):
    return [Conn(r["net"], r["ref"], int(r["pin"]), r["pin_name"] or "",
                 r["sheet"], r["note"] or "") for r in rows(path)]


def load(parts_path, pinouts_path, nets_path):
    return load_parts(parts_path), load_pinouts(pinouts_path), load_nets(nets_path)


def check_conn(c, parts, pinouts, seen):
    """The problems of one pin on one net."""
    where = f"{c.ref}.{c.pin} on '{c.net}' (sheet {c.sheet})"
    part = parts.get(c.ref)
    if part is None:
        return [Problem(True, f"{where}: no part {c.ref} in parts.csv")]
    if not 1 <= c.pin <= part.pins:
        return [Problem(True, f"{where}: {c.ref} has pins 1..{part.pins}")]
    problems = []
    key = (c.ref, c.pin)
    if key in seen and seen[key] != c.net:
        problems.append(Problem(True, f"{where}: already on '{seen[key]}'"))
    seen.setdefault(key, c.net)
    power = part.power.get(c.pin)
    if power and power != c.net:
        problems.append(Problem(True, f"{where}: a {power} pin of {part.type}"))
    expected = pinouts.get(part.type, {}).get(c.pin)
    if c.pin_name and expected and c.pin_name != expected:
        problems.append(Problem(True, f"{where}: drawn '{c.pin_name}', "
                                      f"{part.type} pin {c.pin} is '{expected}'"))
    return problems


def check(parts, pinouts, conns):
    problems = []
    seen = {}
    for c in conns:
        problems += check_conn(c, parts, pinouts, seen)
    used_types = {p.type for p in parts.values()}
    for t in sorted(set(pinouts) - used_types):
        problems.append(Problem(True, f"pinout of {t}: no part has that type"))
    nets = defaultdict(list)
    for c in conns:
        nets[c.net].append(c)
    for name, members in nets.items():
        if len({(c.ref, c.pin) for c in members}) < 2 and \
                not any("open" in c.note for c in members):
            problems.append(Problem(False, f"net '{name}' has one pin "
                                           f"({members[0].ref}.{members[0].pin})"))
    return problems


def main(argv):
    sys.stdout.reconfigure(encoding="utf-8")
    command = argv[1] if len(argv) > 1 else "check"
    parts, pinouts, conns = load(*(os.path.join(ROOT, n) for n in
                                   ("parts.csv", "pinouts.csv", "nets.csv")))
    if command == "stats":
        print(f"{len(parts)} parts, {len({c.net for c in conns})} nets, "
              f"{len(conns)} pins on nets, {len(pinouts)} pinouts")
        return 0
    problems = check(parts, pinouts, conns)
    for p in problems:
        print(("error: " if p.error else "warning: ") + p.text)
    return 1 if any(p.error for p in problems) else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
