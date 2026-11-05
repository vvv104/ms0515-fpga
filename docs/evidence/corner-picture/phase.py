"""From run_board's edges= lines: how many master ticks after a rise of
ТС15 Н each change of the picture's outputs (V), of the attribute registers
(D55:D56) and of ДПСЛ comes."""
import sys
from collections import Counter

rise = None
was = None
hist = {"V": Counter(), "attr": Counter(), "DPSL": Counter(), "BORDS": Counter()}
for line in open(sys.argv[1], encoding="utf-8", errors="replace"):
    if not line.startswith("edges: ") or "tick" in line:
        continue
    f = line.split()
    t = int(f[1])
    now = {"BORDS": f[2], "attr": f[13], "TS15": f[15], "DPSL": f[16], "V": f[17]}
    if was is not None:
        if now["TS15"] == "1" and was["TS15"] == "0":
            rise = t
        if rise is not None:
            for k in hist:
                if now[k] != was[k]:
                    hist[k][t - rise] += 1
    was = now
for k, h in hist.items():
    print(k, "changes, ticks after the rise of TS15:", sorted(h.items()))
