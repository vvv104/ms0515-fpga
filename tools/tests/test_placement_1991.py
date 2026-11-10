"""netlist/placement-1991.csv: every pad of every part but the DIPs on a hole of the 1991 board, each hole once."""
import csv
import json
import os
import sys
import unittest
from collections import Counter

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
import netlist as nl  # noqa: E402

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
NET = os.path.join(ROOT, "netlist")


def rows():
    return list(nl.rows(os.path.join(NET, "placement-1991.csv")))


class Placement(unittest.TestCase):
    def setUp(self):
        self.rows = rows()
        self.holes = json.load(open(os.path.join(NET, "copper.json"), encoding="utf-8"))["pads"]

    def test_a_pad_on_a_hole_stands_at_the_hole(self):
        for r in self.rows:
            if r["source"].startswith("hole "):
                x, y = self.holes[int(r["source"].split()[1])]
                self.assertAlmostEqual(float(r["x"]), x, delta=0.06, msg=f"{r['ref']}.{r['pad']}")
                self.assertAlmostEqual(float(r["y"]), y, delta=0.06, msg=f"{r['ref']}.{r['pad']}")

    def test_no_hole_takes_two_pads_nor_a_dip_pin(self):
        used = Counter(r["source"] for r in self.rows if r["source"].startswith("hole "))
        self.assertEqual([h for h, n in used.items() if n > 1], [])
        dips = {p for fp in json.load(open(os.path.join(NET, "footprints.json"), encoding="utf-8"))
                for p in fp["pads"]}
        self.assertEqual(sorted(h for h in used if int(h.split()[1]) in dips), [])

    def test_only_the_lugs_and_xs1_c10_stand_off_the_holes(self):
        off = sorted(f"{r['ref']}.{r['pad']} {r['source']}" for r in self.rows if not r["source"].startswith("hole "))
        self.assertEqual(off, ["XS1.c10 geometry", "XT1.1 drawing", "XT2.1 drawing", "XT3.1 drawing"])

    def test_every_part_but_the_chips_and_the_boards_own_is_placed(self):
        placed = {r["ref"] for r in self.rows}
        with open(os.path.join(NET, "parts.csv"), encoding="utf-8") as f:
            parts = [p["ref"] for p in csv.DictReader(line for line in f if not line.startswith("#"))]
        dips = {r["ref"] for r in nl.rows(os.path.join(NET, "placement.csv"))} | {"XS3", "XS4", "XS5", "XS6"}
        self.assertEqual(sorted(set(parts) - dips - placed), [])


if __name__ == "__main__":
    unittest.main()
