#!/usr/bin/env python3
"""Compare netlist/nets.csv with Mick's reading of Э3 (netlist/mick.csv,
made by tools/mick_pdf.py).

    python tools/compare_mick.py

Only pins both readings have are compared, and power nets are left out.
For every net of nets.csv: the pins Mick puts elsewhere ("split") and,
for every net of Mick's, the pins nets.csv puts elsewhere ("merged").
Net names are compared without spaces.  Writes netlist/compare-mick.txt.
"""
import csv
import os
from collections import defaultdict

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
NET = os.path.join(ROOT, "netlist")
POWER = {"GND", "+5V", "+12V", "-12V"}


def load(name):
    with open(os.path.join(NET, name), encoding="utf-8") as f:
        rows = [r for r in csv.DictReader(l for l in f if not l.startswith("#"))]
    return {(r["ref"], int(r["pin"])): r["net"] for r in rows}


def groups(pins):
    g = defaultdict(set)
    for k, n in pins.items():
        g[n].add(k)
    return g


def renumber(ours, mick):
    """Mick's library numbers some types' pins its own way (КР580ВИ53 has
    D0 on pin 1 where the chip and Э3 have it on 8).  Per part type, a
    Mick pin whose named net is, on most parts of the type, our pin q's
    net is taken as pin q.  Returns Mick's pins renumbered and the
    per-type maps that were not the identity."""
    with open(os.path.join(NET, "parts.csv"), encoding="utf-8") as f:
        kind = {r["ref"]: r["type"] for r in csv.DictReader(l for l in f if not l.startswith("#"))}
    by_net = defaultdict(set)
    for (ref, pin), net in ours.items():
        by_net[(ref, net.replace(" ", ""))].add(pin)
    votes = defaultdict(lambda: defaultdict(int))
    for (ref, pin), net in mick.items():
        if net.startswith("~") or net in POWER:
            continue
        same = by_net.get((ref, net.replace(" ", "")), ())
        if len(same) == 1:                     # a net the part has on one pin only
            votes[(kind.get(ref), pin)][next(iter(same))] += 1
    maps = defaultdict(dict)
    for (t, pin), v in votes.items():
        q, n = max(v.items(), key=lambda kv: kv[1])
        if n >= 2 or sum(v.values()) == n:
            maps[t][pin] = q
    out = {(ref, maps[kind.get(ref)].get(pin, pin)): net for (ref, pin), net in mick.items()}
    moved = {t: m for t, m in maps.items() if any(a != b for a, b in m.items())}
    return out, moved


def main():
    ours, mick = load("nets.csv"), load("mick.csv")
    mick, moved = renumber(ours, mick)
    both = {k for k in ours if k in mick and ours[k] not in POWER and mick[k] not in POWER}
    ours_g = groups({k: ours[k] for k in both})
    mick_g = groups({k: mick[k] for k in both})
    lines, differ = [], 0
    for net, pins in sorted(ours_g.items()):
        by_mick = defaultdict(list)
        for k in pins:
            by_mick[mick[k]].append(f"{k[0]}.{k[1]}")
        if len(by_mick) > 1:
            differ += 1
            parts = "; ".join(f"{m}: {' '.join(sorted(v))}" for m, v in sorted(by_mick.items()))
            lines.append(f"split  {net}  ->  {parts}")
    for net, pins in sorted(mick_g.items()):
        by_ours = defaultdict(list)
        for k in pins:
            by_ours[ours[k]].append(f"{k[0]}.{k[1]}")
        if len(by_ours) > 1:
            differ += 1
            parts = "; ".join(f"{o}: {' '.join(sorted(v))}" for o, v in sorted(by_ours.items()))
            lines.append(f"merged {net}  <-  {parts}")
    names = sum(1 for k in both if ours[k].replace(" ", "") == mick[k].replace(" ", ""))
    head = (f"{len(both)} signal pins in both readings, {names} with the same net name, "
            f"{differ} disagreements")
    lines += [f"renumbered {t}: " + " ".join(f"{a}->{b}" for a, b in sorted(m.items()) if a != b)
              for t, m in sorted(moved.items(), key=lambda kv: str(kv[0]))]
    with open(os.path.join(NET, "compare-mick.txt"), "w", encoding="utf-8") as f:
        f.write(head + "\n" + "\n".join(lines) + "\n")
    print(head)


if __name__ == "__main__":
    main()
