#!/usr/bin/env python3
"""The НС4 netlist: load netlist/*.csv and check it.

    python tools/netlist.py check   - list the problems (exit 1 on errors)
    python tools/netlist.py stats   - parts, nets and pins read so far
    python tools/netlist.py check rev3a - the same with a board's changes
                                    (netlist/rev3a.csv) applied over Э3

netlist/<board>.csv  ref,pin,from_net,to_net,evidence - the changes a real
                     board makes to Э3: a pin moved off one net onto
                     another; every change names the evidence it rests on

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


@dataclass
class Change:
    ref: str
    pin: int
    from_net: str
    to_net: str
    evidence: str


def load_changes(path):
    return [Change(r["ref"], int(r["pin"]), r["from_net"], r["to_net"], r["evidence"] or "")
            for r in rows(path)]


def apply_changes(conns, changes):
    """The pins moved as a board's change list says: each change takes a pin
    off the net Э3 has it on and puts it on another.  Returns the new pin
    list and the problems (a pin not where the change expects it, a change
    with no evidence)."""
    out, errors = list(conns), []
    for ch in changes:
        where = f"{ch.ref}.{ch.pin} '{ch.from_net}' -> '{ch.to_net}'"
        if not ch.evidence.strip():
            errors.append(Problem(True, f"change {where}: no evidence"))
            continue
        hit = [i for i, c in enumerate(out)
               if (c.ref, c.pin) == (ch.ref, ch.pin) and c.net == ch.from_net]
        if not hit:
            errors.append(Problem(True, f"change {where}: the pin is not on '{ch.from_net}'"))
            continue
        for i in hit:
            c = out[i]
            out[i] = Conn(ch.to_net, c.ref, c.pin, c.pin_name, c.sheet,
                          f"board: {ch.evidence}")
    return out, errors


def main(argv):
    sys.stdout.reconfigure(encoding="utf-8")
    command = argv[1] if len(argv) > 1 else "check"
    board = argv[2] if len(argv) > 2 else None
    parts, pinouts, conns = load(*(os.path.join(ROOT, n) for n in
                                   ("parts.csv", "pinouts.csv", "nets.csv")))
    if board:
        conns, errors = apply_changes(conns, load_changes(os.path.join(ROOT,
                                                                       f"{board}.csv")))
        for p in errors:
            print("error: " + p.text)
        if errors:
            return 1
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
