"""tools/readback.py on made-up copper: the board's nets read off its geometry, against the netlist."""
import os
import sys
import unittest

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
import readback  # noqa: E402


def pad(name, x, y, r=0.8):
    return {"pin": name, "x": x, "y": y, "r": r}


def track(layer, x0, y0, x1, y1, w=0.25):
    return {"layer": layer, "x0": x0, "y0": y0, "x1": x1, "y1": y1, "w": w}


class Groups(unittest.TestCase):
    def test_copper_that_touches_is_one_group_names_or_none(self):
        pads = [pad("D1.1", 0, 0), pad("D2.1", 10, 0), pad("D3.1", 0, 10)]
        tracks = [track("F.Cu", 0, 0, 5, 0), track("F.Cu", 5, 0, 10, 0)]
        self.assertEqual(readback.pin_groups(pads, tracks, [], []), [{"D1.1", "D2.1"}, {"D3.1"}])

    def test_a_via_joins_the_layers_a_track_crossing_another_joins_it(self):
        pads = [pad("D1.1", 0, 0), pad("D2.1", 10, 10), pad("D3.1", 5, -5)]
        tracks = [track("F.Cu", 0, 0, 5, 5), track("B.Cu", 5, 5, 10, 10),
                  track("B.Cu", 5, -5, 5, 20)]                      # passes through the via's copper
        vias = [{"x": 5, "y": 5, "r": 0.5}]
        self.assertEqual(readback.pin_groups(pads, tracks, vias, []), [{"D1.1", "D2.1", "D3.1"}])

    def test_tracks_on_two_layers_do_not_touch(self):
        pads = [pad("D1.1", 0, 0), pad("D2.1", 5, -5)]
        tracks = [track("F.Cu", 0, 0, 10, 0), track("B.Cu", 5, -5, 5, 5)]
        self.assertEqual(len(readback.pin_groups(pads, tracks, [], [])), 2)

    def test_a_plane_joins_the_pads_it_reaches(self):
        pads = [pad("D1.7", 0, 0), pad("D2.7", 50, 0), pad("D3.1", 25, 0)]
        planes = [{"pins": {"D1.7", "D2.7"}}]                       # what the fill overlaps
        self.assertEqual(readback.pin_groups(pads, [], [], planes), [{"D1.7", "D2.7"}, {"D3.1"}])


class Compare(unittest.TestCase):
    def test_equal_partitions_have_no_difference(self):
        want = {"D1.1": "A", "D2.1": "A", "D3.1": "B"}
        self.assertEqual(readback.compare([{"D1.1", "D2.1"}, {"D3.1"}], want), [])

    def test_a_net_in_two_pieces_and_two_nets_in_one_are_told(self):
        want = {"D1.1": "A", "D2.1": "A", "D3.1": "B", "D4.1": "C"}
        diffs = readback.compare([{"D1.1"}, {"D2.1"}, {"D3.1", "D4.1"}], want)
        self.assertEqual(diffs, ["A: in 2 pieces: D1.1 | D2.1", "B, C joined: D3.1, D4.1"])

    def test_a_pin_on_no_net_joined_to_one_is_told(self):
        diffs = readback.compare([{"D1.1", "D9.9"}], {"D1.1": "A"})
        self.assertEqual(diffs, ["A, (no net) joined: D1.1, D9.9"])


if __name__ == "__main__":
    unittest.main()
