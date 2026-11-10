"""tools/lay1991.py on made-up copper: nets from the holes along the copper, cuts, stubs."""
import os
import sys
import unittest

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
import lay1991  # noqa: E402

# holes 0 and 1 on two nets, hole 2 a via (no pad), 120 px apart in a row
HOLES = [(0.0, 0.0), (120.0, 0.0), (240.0, 0.0)]


def line(pts, holes, w=6.0):
    return {"w": w, "pts": [list(p) for p in pts], "holes": holes}


class Label(unittest.TestCase):
    def test_a_line_takes_the_net_of_the_hole_its_copper_reaches_and_runs_on_to_its_centre(self):
        lines = [line([(16, 0), (100, 0)], [0])]
        out = lay1991.label(lines, HOLES, {0: "A"}, {})
        self.assertEqual(len(out), 1)
        net, w, pts = out[0]
        self.assertEqual(net, "A")
        self.assertEqual(pts[0], [0.0, 0.0])

    def test_a_net_goes_on_through_a_line_of_lines(self):
        lines = [line([(16, 0), (60, 0)], [0]), line([(60, 0), (60, 50)], [0]), line([(60, 50), (60, 90)], [0])]
        out = lay1991.label(lines, HOLES, {0: "A"}, {})
        self.assertEqual([n for n, _, _ in out], ["A", "A", "A"])

    def test_a_hole_the_copper_does_not_reach_is_not_joined_however_near(self):
        lines = [line([(16, 0), (104, 0)], [0])]          # ends 16 px from hole 1, but its area misses it
        out = lay1991.label(lines, HOLES, {0: "A", 1: "B"}, {})
        self.assertEqual(out[0][2][-1], [104, 0])

    def test_where_two_nets_meet_the_line_between_them_is_cut(self):
        lines = [line([(16, 0), (50, 0)], [0, 1]), line([(50, 0), (70, 0)], [0, 1]), line([(70, 0), (104, 0)], [0, 1])]
        out = lay1991.label(lines, HOLES, {0: "A", 1: "B"}, {})
        self.assertEqual([(n, p[0][0]) for n, _, p in out], [("A", 0.0), ("B", 70)])
        lines = [line([(16, 0), (104, 0)], [0, 1])]
        self.assertEqual(lay1991.label(lines, HOLES, {0: "A", 1: "B"}, {}), [])

    def test_a_via_passes_its_copper_groups_net_on(self):
        lines = [line([(224, 0), (180, 0)], [2])]
        out = lay1991.label(lines, HOLES, {}, {2: "C"})
        self.assertEqual(out[0][0], "C")

    def test_copper_that_reaches_no_netted_hole_is_left_out(self):
        self.assertEqual(lay1991.label([line([(300, 0), (400, 0)], [])], HOLES, {}, {}), [])


class Layers(unittest.TestCase):
    def test_a_net_crosses_layers_through_a_via(self):
        layers = {"top": [line([(16, 0), (224, 0)], [0, 2])], "bottom": [line([(256, 0), (400, 0)], [2])]}
        out = lay1991.label_layers(layers, HOLES, {0: "A"}, {})
        self.assertEqual([t[0] for t in out["bottom"]], ["A"])

    def test_two_nets_meeting_at_a_via_from_two_layers_are_cut_there(self):
        holes = HOLES + [(480.0, 0.0)]
        layers = {"top": [line([(16, 0), (100, 0)], [0, 2]), line([(100, 0), (224, 0)], [0, 2])],
                  "bottom": [line([(256, 0), (380, 0)], [2, 3]), line([(380, 0), (464, 0)], [2, 3])]}
        out = lay1991.label_layers(layers, holes, {0: "A", 3: "B"}, {})
        nets = sorted(t[0] for runs in out.values() for t in runs)
        self.assertEqual(len(nets), 3)
        self.assertEqual(set(nets), {"A", "B"})


class Stubs(unittest.TestCase):
    def test_a_short_run_on_a_neighbours_copper_is_taken_off_a_long_one_kept(self):
        tracks = {"L": [["A", 6.0, [[0, 0], [100, 0]]],
                        ["B", 6.0, [[50, 3], [60, 3]]],          # 10 px, 3 px from A's run: a stub
                        ["C", 6.0, [[0, 40], [100, 40]]]]}
        kept, gone = lay1991.drop_stubs(tracks, [], {}, stub_px=31, gap_px=3.1)
        self.assertEqual([t[0] for t in kept["L"]], ["A", "C"])
        self.assertEqual(gone, 1)

    def test_a_short_run_on_another_nets_hole_is_taken_off(self):
        tracks = {"L": [["A", 6.0, [[0, 0], [20, 0]]]]}
        kept, gone = lay1991.drop_stubs(tracks, [(25.0, 0.0)], {0: "B"}, stub_px=31, gap_px=3.1, hole_r_px=8)
        self.assertEqual(kept["L"], [])
        self.assertEqual(gone, 1)


class Vias(unittest.TestCase):
    def test_a_via_on_every_padless_hole_a_track_ends_on_with_the_tracks_net(self):
        tracks = {"L": [["A", 6.0, [[0, 0], [120, 0]]], ["A", 6.0, [[240, 0], [300, 0]]]]}
        self.assertEqual(lay1991.vias(tracks, HOLES, {0, 1}), {2: "A"})

    def test_a_stub_on_a_via_loses_it_to_the_longer_run(self):
        tracks = {"L": [["B", 6.0, [[230, 0], [240, 0]]], ["A", 6.0, [[240, 0], [340, 0]]]],
                  "M": [["B", 6.0, [[240, 0], [240, 15]]]]}
        self.assertEqual(lay1991.vias(tracks, HOLES, {0, 1}), {2: "A"})


if __name__ == "__main__":
    unittest.main()
