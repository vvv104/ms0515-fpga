"""tools/copper1991.py on made-up layer images: centre lines, their widths, the holes their copper reaches."""
import math
import os
import sys
import unittest

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
try:
    import cv2
    import numpy as np
except ImportError:                     # plain Python with the board tools' packages only
    cv2 = None
if cv2:
    import copper1991  # noqa: E402

COPPER, RING = 16, 65                   # palette indices as Mick's images have them (copper.py)


def blank(w=300, h=200):
    return np.zeros((h, w), np.uint8)


@unittest.skipUnless(cv2, "no OpenCV")
class Lines(unittest.TestCase):
    def test_a_run_from_a_ring_is_one_line_reaching_that_hole(self):
        im = blank()
        cv2.circle(im, (40, 100), 11, RING, thickness=3)
        cv2.line(im, (52, 100), (250, 100), COPPER, thickness=5)
        lines = copper1991.layer_lines(im, [(40.0, 100.0)])
        self.assertEqual(len(lines), 1)
        ln = lines[0]
        self.assertEqual(ln["holes"], [0])
        xs = sorted(p[0] for p in (ln["pts"][0], ln["pts"][-1]))
        self.assertLess(xs[0], 60)
        self.assertGreater(xs[1], 240)
        self.assertAlmostEqual(ln["w"], int((im[:, 150] == COPPER).sum()), delta=1)

    def test_a_crossing_is_four_lines_from_its_middle(self):
        im = blank()
        cv2.line(im, (50, 100), (250, 100), COPPER, thickness=5)
        cv2.line(im, (150, 20), (150, 180), COPPER, thickness=5)
        lines = copper1991.layer_lines(im, [])
        self.assertEqual(len(lines), 4)
        for ln in lines:
            ends = [ln["pts"][0], ln["pts"][-1]]
            self.assertTrue(any(math.dist(p, (150, 100)) < 4 for p in ends))

    def test_copper_reaching_no_ring_has_no_holes(self):
        im = blank()
        cv2.circle(im, (40, 100), 11, RING, thickness=3)
        cv2.line(im, (100, 150), (250, 150), COPPER, thickness=5)
        self.assertEqual(copper1991.layer_lines(im, [(40.0, 100.0)])[0]["holes"], [])


if __name__ == "__main__":
    unittest.main()
