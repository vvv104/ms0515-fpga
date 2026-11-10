"""kicad/ns4.pretty: the footprints of the 1991 board's parts, as tools/pretty.py writes them."""
import math
import os
import sys
import unittest

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
import pretty  # noqa: E402


def pads_of(text):
    """{pad: (x, y)} of a footprint's text."""
    out = {}
    for line in text.splitlines():
        line = line.strip()
        if line.startswith("(pad "):
            name = line.split('"')[1]
            at = line.split("(at ")[1].split(")")[0].split()
            out[name] = (float(at[0]), float(at[1]))
    return out


class Footprints(unittest.TestCase):
    def setUp(self):
        self.made = pretty.footprints()

    def test_two_pin_parts_at_the_1991_pitches(self):
        for name, pitch in (("R_Axial_P12.70mm", 12.7), ("C_Disc_P5.08mm", 5.08), ("Wire_Link_P12.70mm", 12.7),
                            ("CP_Axial_P25.40mm", 25.4), ("C_Rect_P10.16mm", 10.16),
                            ("Crystal_RK169_P5.08mm", 5.08), ("Crystal_RK353_P12.70mm", 12.7)):
            p = pads_of(self.made[name])
            self.assertEqual(sorted(p), ["1", "2"], name)
            self.assertAlmostEqual(math.dist(p["1"], p["2"]), pitch, places=2, msg=name)

    def test_the_trimmer_has_its_wiper_beside_pin_1_and_pin_2_below_it(self):
        p = pads_of(self.made["Trimmer_SP5-16VA"])
        self.assertAlmostEqual(math.dist(p["1"], p["3"]), 2.54, places=2)
        self.assertAlmostEqual(math.dist(p["2"], p["3"]), 3.175, places=2)

    def test_the_d_subs_take_their_pins_off_the_1991_holes(self):
        for name, n in (("DSUB-9_ONP-ZhG-79", 9), ("DSUB-15_ONP-ZhG-79", 15), ("DSUB-25_ONP-ZhG-79", 25)):
            p = pads_of(self.made[name])
            self.assertEqual(sorted(p, key=int), [str(i) for i in range(1, n + 1)], name)
            self.assertEqual(p["1"], (0.0, 0.0), name)

    def test_the_library_is_what_it_writes(self):
        self.assertEqual(pretty.stale(), [])


if __name__ == "__main__":
    unittest.main()
