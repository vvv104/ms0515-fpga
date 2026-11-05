"""Where two frames of run_board differ: counts by column and row, and a
picture with the differing samples in red over the first frame."""
import sys
from collections import Counter

from PIL import Image

a, b, out = sys.argv[1:4]
x, y = Image.open(a).convert("RGB"), Image.open(b).convert("RGB")
print(a, x.size, b, y.size)
w, h = min(x.width, y.width), min(x.height, y.height)
px, py = x.load(), y.load()
cols, rows, n = Counter(), Counter(), 0
mark = x.copy()
pm = mark.load()
for j in range(h):
    for i in range(w):
        if px[i, j] != py[i, j]:
            n += 1
            cols[i] += 1
            rows[j] += 1
            pm[i, j] = (255, 0, 0)
print("differ:", n)
print("columns:", sorted(cols.items())[:12], "...", len(cols), "columns")
print("rows:", sorted(rows.items())[:8], "...", len(rows), "rows")
print("column period:", Counter((c2 - c1) for c1, c2 in zip(sorted(cols), sorted(cols)[1:])).most_common(4))
mark.resize((w * 2, h * 3), Image.NEAREST).save(out)
