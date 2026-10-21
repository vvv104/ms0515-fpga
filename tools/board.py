#!/usr/bin/env python3
"""Generate the board, rtl/board/ns4.sv, from the netlist.

    python tools/board.py          - write rtl/board/ns4.sv, list the problems
    python tools/board.py check    - the same without writing (exit 1 when
                                     the committed file is stale or wrong)

The board is the chips of netlist/parts.csv wired by netlist/nets.csv, with
the owner's board's changes (netlist/rev3a.csv) applied:

- a chip whose type has a module in rtl/chips/ is an instance of it; a chip
  with none yet (the processor, the large chips) and a connector are
  brought out: each of their signal pins is a pair of board inputs
  x_<ref>_p<n> / x_<ref>_p<n>_oe that drive the net while _oe is 1, so a
  test bench stands in for them;
- every net is a board output n_<name>, its level resolved from its
  drivers: one push-pull output alone; open collectors ANDed; three-state
  outputs and brought-out pins by their enables, with the net's pull (or 1,
  what an undriven TTL input reads) when none drives; the output
  `contention` is 1 while two drivers of a net are on at once;
- every part that is not a chip (resistors, capacitors, diodes, jumpers,
  switches, connectors) needs a treatment, from netlist/analog.csv or one
  of two obvious rules: a part with all its pins on power is left out
  (decoupling), a resistor with a pin on +5V pulls its other nets up.

netlist/analog.csv   ref,treatment,pins,reason
    short / wire   joins the nets of the pin pairs in `pins` ("1-2 3-18";
                   empty for 1-2): a fitted jumper, a closed switch, a part
                   standing for a conductor
    open           left out: not fitted, or no effect on the logic
    pull0 / pull1  pulls the part's signal nets low / high
    port           a connector: its signal pins brought out
    pending        brought out like a connector until it has a model
"""
import csv
import os
import re
import sys
from collections import defaultdict
from dataclasses import dataclass, field

import netlist as nl

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.join(HERE, "..")
POWER = {"GND": "1'b0", "+5V": "1'b1", "+12V": None, "-12V": None}
ZERO = "1'b0"
SUPPLIES = {"VCC", "VEE", "VDD", "VBB", "VSS", "GND", "BGND"}
RESISTORS = ("С1-4", "С2-23", "НР1-11", "СП5-16")
TRANSLIT = dict(zip("АБВГДЕЗИЙКЛМНОПРСТУФХЦЫЭ", "ABVGDEZIYKLMNOPRSTUFHCYE"))
TRANSLIT.update({"Ж": "ZH", "Ч": "CH", "Ш": "SH", "Щ": "SCH", "Ю": "YU", "Я": "YA",
                 "Ь": "", "Ъ": "", "Ё": "E"})


def translit(text):
    return "".join(TRANSLIT.get(ch.upper(), ch) for ch in text)


def ident(net):
    """The SystemVerilog name of a net: n_<name>, nx_ for the unnamed ~ nets."""
    prefix = "nx_" if net.startswith("~") else "n_"
    body = re.sub(r"[^A-Za-z0-9]+", "_", translit(net.lstrip("~"))).strip("_")
    return prefix + body


def module_name(chip_type):
    """К555ЛА3 -> k555la3; a grade letter after the last digit is dropped."""
    return translit(re.sub(r"(?<=\d)[А-Я]$", "", chip_type)).lower()


def is_chip(part_type):
    return re.match(r"^КР?\d{3,4}[А-Я]{2}\d", part_type) is not None


@dataclass
class Module:
    name: str
    ports: dict
    open_collector: bool


def closing(text, start):
    """The index of the parenthesis closing one opened just before `start`."""
    depth = 1
    for i in range(start, len(text)):
        depth += {"(": 1, ")": -1}.get(text[i], 0)
        if depth == 0:
            return i
    raise ValueError("unbalanced parentheses")


def parse_ports(text):
    """The ports of a module's ANSI port list (the text after its name):
    each name with its direction, a list like `input logic a, b` included."""
    body = re.sub(r"//[^\n]*", "", text).lstrip()
    if body.startswith("#"):                    # a parameter list comes first
        body = body[closing(body, body.index("(") + 1) + 1:]
    body = body[body.index("(") + 1:]
    end = closing(body, 0)
    ports, way = {}, None
    for item in body[:end].split(","):
        words = re.findall(r"\w+", re.sub(r"\[[^]]*\]", "", item))
        if not words:
            continue
        if words[0] in ("input", "output"):
            way = words[0]
        ports[words[-1]] = way
    return ports


def load_modules(folder):
    modules = {}
    for fname in sorted(os.listdir(folder)):
        if not fname.endswith(".sv"):
            continue
        with open(os.path.join(folder, fname), encoding="utf-8") as f:
            text = f.read()
        m = re.search(r"^module\s+(\w+)", text, re.M)
        if not m:
            continue
        header = text[:m.start()].lower()
        ports = parse_ports(text[m.end():])
        modules[m.group(1)] = Module(m.group(1), ports, "open-collector" in header or
                                     "open collector" in header)
    return modules


@dataclass
class Treatment:
    ref: str
    kind: str
    pins: str
    reason: str


def load_analog(path):
    return {r["ref"]: Treatment(r["ref"], r["treatment"], r["pins"] or "", r["reason"] or "")
            for r in nl.rows(path)}


@dataclass
class Sources:
    parts: str
    pinouts: str
    nets: str
    changes: str
    analog: str
    chips: str
    firmware: str = None        # ref,file,format; files relative to the folder above it


def ns4_sources():
    n = os.path.join(REPO, "netlist")
    return Sources(os.path.join(n, "parts.csv"), os.path.join(n, "pinouts.csv"),
                   os.path.join(n, "nets.csv"), os.path.join(n, "rev3a.csv"),
                   os.path.join(n, "analog.csv"), os.path.join(REPO, "rtl", "chips"),
                   os.path.join(n, "firmware.csv"))


def contents(data, fmt):
    """A chip's CONTENTS parameter from its image: word a in the bits from
    w*a up, w the word's width."""
    if fmt == "nibbles":
        words, width = [b & 0xF for b in data], 4
    elif fmt == "bytes":
        words, width = list(data), 8
    elif fmt in ("even-bytes", "odd-bytes"):        # one chip of a 16-bit pair
        words, width = list(data[fmt == "odd-bytes"::2]), 8
    else:
        raise ValueError(f"firmware format '{fmt}'")
    digits = width // 4
    text = "".join(f"{w:0{digits}x}" for w in reversed(words))
    return f"{len(words) * width}'h{text}"


def load_firmware(path):
    """{ref: the CONTENTS literal} from firmware.csv."""
    if not path:
        return {}
    root = os.path.dirname(os.path.dirname(os.path.abspath(path)))
    out = {}
    for r in nl.rows(path):
        with open(os.path.join(root, r["file"]), "rb") as f:
            out[r["ref"]] = contents(f.read(), r["format"])
    return out


class Joins:
    """Nets joined by shorts: union-find, a power net or a named net wins."""

    def __init__(self):
        self.up = {}

    def find(self, net):
        while self.up.get(net, net) != net:
            net = self.up[net]
        return net

    def join(self, a, b):
        a, b = self.find(a), self.find(b)
        if a == b:
            return
        rank = lambda n: (n not in POWER, n.startswith("~"), n)
        keep, drop = sorted((a, b), key=rank)
        self.up[drop] = keep


@dataclass
class Net:
    name: str
    pp: list = field(default_factory=list)       # push-pull outputs
    oc: list = field(default_factory=list)       # open collectors
    tri: list = field(default_factory=list)      # (value, enable)
    pulls: set = field(default_factory=set)
    readers: int = 0


class Problems(list):
    def error(self, text):
        self.append(nl.Problem(True, text))

    def warn(self, text):
        self.append(nl.Problem(False, text))


def pin_pairs(spec):
    pairs = []
    for item in (spec or "1-2").split():
        a, b = item.split("-")
        pairs.append((int(a), int(b)))
    return pairs


def treatment_of(part, pins, analog):
    """The treatment of a part that is not a chip: analog.csv, or a rule."""
    if part.ref in analog:
        return analog[part.ref]
    if all(net in POWER for net in pins.values()):
        return Treatment(part.ref, "open", "", "all its pins on power")
    if part.type.startswith(RESISTORS) and "+5V" in pins.values():
        return Treatment(part.ref, "pull1", "", "a resistor to +5V")
    return None


class Builder:
    def __init__(self, src):
        self.problems = Problems()
        self.parts, self.pinouts, conns = nl.load(src.parts, src.pinouts, src.nets)
        if src.changes:
            conns, errors = nl.apply_changes(conns, nl.load_changes(src.changes))
            self.problems.extend(errors)
        self.analog = load_analog(src.analog)
        self.modules = load_modules(src.chips)
        self.firmware = load_firmware(src.firmware)
        self.pins = defaultdict(dict)
        for c in conns:
            self.pins[c.ref][c.pin] = c.net
        self.joins = Joins()
        self.nets = {}
        self.instances = []     # (ref, module, [(port, expr)])
        self.ports = []         # brought-out inputs, in order
        self.out = []           # (ref, why) parts brought out
        self.wires = []         # internal driver wires
        self.tied = []          # chip inputs on no net

    def treatments(self):
        found = {}
        for ref, part in self.parts.items():
            if is_chip(part.type):
                continue
            t = treatment_of(part, self.pins.get(ref, {}), self.analog)
            if t is None:
                self.problems.error(f"{ref} ({part.type}): no treatment in netlist/analog.csv")
                continue
            found[ref] = t
            if t.kind in ("short", "wire"):
                for a, b in pin_pairs(t.pins):
                    na, nb = self.pins[ref].get(a), self.pins[ref].get(b)
                    if na and nb:
                        self.joins.join(na, nb)
        for ref in self.analog:
            if ref not in self.parts:
                self.problems.error(f"analog.csv: no part {ref}")
        return found

    def net(self, name):
        name = self.joins.find(name)
        if name not in self.nets:
            self.nets[name] = Net(name)
        return self.nets[name]

    def bring_out(self, ref, why):
        self.out.append((ref, why))
        for pin, name in sorted(self.pins.get(ref, {}).items()):
            if self.joins.find(name) in POWER or self.supply(ref, pin):
                continue
            port = f"x_{ref}_p{pin}"
            self.ports += [port, port + "_oe"]
            self.net(name).tri.append((port, port + "_oe"))
            self.net(name).readers += 1

    def place_chip(self, ref, part):
        module = self.modules.get(module_name(part.type))
        if module is None:
            self.bring_out(ref, f"{part.type}: no module yet")
            return
        conns = []
        pins = self.pins.get(ref, {})
        if "clk" in module.ports:
            conns.append(("clk", "clk"))
        for port, way in module.ports.items():
            m = re.fullmatch(r"p(\d+)(_in)?", port)     # pN_in: a driven pin's level
            if not m:
                continue
            pin = int(m.group(1))
            name = pins.get(pin) or f"~{ref}.{pin}"
            if way == "input":
                if pin not in pins and not m.group(2):
                    self.tied.append(f"{ref}.{pin}")
                    conns.append((port, "1'b1"))
                    continue
                conns.append((port, self.read(name, ref, pin)))
                continue
            wire = f"d_{ref}_{port}"
            conns.append((port, wire))
            self.wires.append(wire)
            net = self.net(name)
            if port + "_oe" in module.ports:
                conns.append((port + "_oe", wire + "_oe"))
                self.wires.append(wire + "_oe")
                net.tri.append((wire, wire + "_oe"))
            elif module.open_collector:
                net.oc.append(wire)
            else:
                net.pp.append(wire)
        for pin in sorted(pins):
            if f"p{pin}" not in module.ports and f"p{pin}_in" not in module.ports \
                    and not self.supply(ref, pin):
                self.problems.error(f"{ref}.{pin} on '{pins[pin]}': not a pin of {module.name}")
        self.instances.append((ref, module.name, conns, self.firmware.get(ref)))

    def supply(self, ref, pin):
        """A supply pin left off the power table (VEE of КР1102АП15): no logic."""
        return self.pinouts.get(self.parts[ref].type, {}).get(pin) in SUPPLIES

    def read(self, name, ref, pin):
        root = self.joins.find(name)
        if root in POWER:
            if POWER[root] is None:
                self.problems.error(f"{ref}.{pin}: a logic input on {root}")
                return "1'b1"
            return POWER[root]
        self.net(root).readers += 1
        return ident(root)

    def pull(self, ref, t):
        level = "1'b1" if t.kind == "pull1" else "1'b0"
        for name in self.pins.get(ref, {}).values():
            if self.joins.find(name) not in POWER:
                self.net(name).pulls.add(level)

    def build(self):
        found = self.treatments()
        for ref in sorted(self.parts, key=natural):
            part = self.parts[ref]
            if is_chip(part.type):
                self.place_chip(ref, part)
                continue
            t = found.get(ref)
            if t is None:
                continue
            if t.kind in ("pull0", "pull1"):
                self.pull(ref, t)
            elif t.kind in ("port", "pending"):
                self.bring_out(ref, f"{t.kind}: {t.reason}")
            elif t.kind not in ("short", "wire", "open"):
                self.problems.error(f"{ref}: unknown treatment '{t.kind}'")
        return self


def natural(text):
    return [int(s) if s.isdigit() else s for s in re.split(r"(\d+)", text)]


def resolve(net, problems):
    """The expression of a net's level, and its contention term (or None)."""
    pull = "1'b1"
    if len(net.pulls) > 1:
        problems.error(f"'{net.name}': pulled both up and down")
    elif net.pulls:
        pull = next(iter(net.pulls))
    if net.pp:
        if len(net.pp) > 1 or net.oc:
            problems.error(f"'{net.name}': two outputs drive it: {', '.join(net.pp + net.oc)}")
        # a brought-out pin here can only be an input: it gets no ports
        enables = [oe for v, oe in net.tri if not v.startswith("x_")]
        return net.pp[0], " | ".join(enables) if enables else None
    level, busy = pull, None
    if net.tri:
        enables = [oe for _, oe in net.tri]
        if len(net.tri) == 1:
            value, oe = net.tri[0]
            level = f"{oe} ? {value} : {pull}"
        else:
            chosen = " | ".join(f"({oe} & {v})" for v, oe in net.tri)
            level = f"({' | '.join(enables)}) ? ({chosen}) : {pull}"
            busy = f"$countones({{{', '.join(enables)}}}) > 1"
    elif not net.oc and not net.pulls:
        problems.warn(f"'{net.name}': nothing drives it")
    if net.oc:
        oc = " & ".join(net.oc)
        level = oc if level == "1'b1" else f"{oc} & ({level})"
    return level, busy


def header(b):
    lines = ["// The НС4 board, generated by tools/board.py from netlist/ - do not edit.",
             "// tools/board.py tells what each port and net is.", "//"]
    if b.out:
        lines.append("// Brought out (the test bench stands in for them):")
        lines += [f"//   {ref:5} {why}" for ref, why in b.out]
    if b.tied:
        lines.append("// Chip inputs on no net, tied high: " + " ".join(b.tied))
    return lines


def emit(b, levels, busy):
    """The text of the board module."""
    nets = sorted(levels, key=lambda n: natural(ident(n)))
    fought = [n for n in nets if busy[n]]
    clocked = any(port == "clk" for _, _, conns, _ in b.instances for port, _ in conns)
    ports = (["input  logic clk"] if clocked else []) + \
        [f"input  logic {p}" for p in b.ports] + \
        [f"output logic {ident(n)}" for n in nets] + ["output logic contention"]
    if fought:
        ports.append(f"output logic [{len(fought) - 1}:0] contention_at")
    lines = header(b)
    if fought:
        lines.append("// contention_at, bit by bit: " +
                     ", ".join(f"{i} {n}" for i, n in enumerate(fought)))
    lines += ["module ns4 ("]
    lines += [f"    {p}," for p in ports[:-1]] + [f"    {ports[-1]}", ");"]
    lines += [f"    logic {w};" for w in b.wires]
    for ref, module, conns, image in b.instances:
        lines.append("")
        params = f" #(.CONTENTS({image}))" if image else ""
        lines.append(f"    {module}{params} {ref} (")
        lines += [f"        .{port}({expr})," for port, expr in conns[:-1]]
        port, expr = conns[-1]
        lines += [f"        .{port}({expr})", "    );"]
    lines.append("")
    lines += [f"    assign {ident(n)} = {levels[n]};" for n in nets]
    lines += [f"    assign contention_at[{i}] = {busy[n]};" for i, n in enumerate(fought)]
    lines.append(f"    assign contention = {'|contention_at' if fought else ZERO};")
    lines.append("endmodule")
    return "\n".join(lines) + "\n"


def generate(src):
    """The board's text and the problems found on the way."""
    b = Builder(src).build()
    levels, busy, names = {}, {}, {}
    for name, net in b.nets.items():
        levels[name], busy[name] = resolve(net, b.problems)
        if net.pp:
            inputs = {p for v, oe in net.tri if v.startswith("x_") for p in (v, oe)}
            b.ports = [p for p in b.ports if p not in inputs]
        other = names.setdefault(ident(name), name)
        if other != name:
            b.problems.error(f"'{name}' and '{other}' are both {ident(name)}")
    return emit(b, levels, busy), b.problems


def main(argv):
    sys.stdout.reconfigure(encoding="utf-8")
    text, problems = generate(ns4_sources())
    for p in problems:
        print(("error: " if p.error else "warning: ") + p.text)
    path = os.path.join(REPO, "rtl", "board", "ns4.sv")
    if len(argv) > 1 and argv[1] == "check":
        with open(path, encoding="utf-8") as f:
            stale = f.read() != text
        if stale:
            print("error: rtl/board/ns4.sv is stale")
        return 1 if stale or any(p.error for p in problems) else 0
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "w", encoding="utf-8", newline="\n") as f:
        f.write(text)
    return 1 if any(p.error for p in problems) else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
