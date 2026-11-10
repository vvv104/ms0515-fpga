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


class Scraps(unittest.TestCase):
    def test_runs_that_reach_no_hole_through_each_other_go_the_rest_stay(self):
        tracks = {"L": [["A", 6.0, [[0, 0], [60, 0]]], ["A", 6.0, [[60, 0], [60, 50]]],    # on hole 0
                        ["B", 6.0, [[150, 40], [160, 40]]]]}                                # nowhere
        kept, gone = lay1991.drop_scraps(tracks, HOLES, 8.0)
        self.assertEqual(gone, 1)
        self.assertEqual([t[0] for t in kept["L"]], ["A", "A"])

    def test_a_run_whose_end_lies_on_a_holes_copper_stays(self):
        tracks = {"L": [["A", 6.0, [[125, 3], [125, 60]]]]}          # 6 px off hole 1's centre
        self.assertEqual(lay1991.drop_scraps(tracks, HOLES, 8.0)[1], 0)


class Widths(unittest.TestCase):
    # segments (layer, x0, y0, x1, y1, net) and pads (x, y, r, net), in mm
    def test_a_run_with_room_keeps_its_width(self):
        segs = [("L", 0, 0, 10, 0, "A")]
        self.assertEqual(lay1991.fit_widths(segs, [(5, 3, 0.8, "B")], 0.25, 0.15, 0.2), [0.25])

    def test_a_run_too_near_a_pad_narrows_just_enough(self):
        # 1.1 from the centre of a 0.8 pad: 0.3 - 0.2 leaves 0.1 a side, 0.2 wide
        segs = [("L", 0, 0, 10, 0, "A")]
        self.assertAlmostEqual(lay1991.fit_widths(segs, [(5, 1.1, 0.8, "B")], 0.25, 0.15, 0.2)[0], 0.2)

    def test_two_runs_too_near_share_the_narrowing_and_stop_at_the_floor(self):
        segs = [("L", 0, 0, 10, 0, "A"), ("L", 0, 0.42, 10, 0.42, "B"), ("M", 0, 0.1, 10, 0.1, "C")]
        self.assertEqual([round(w, 3) for w in lay1991.fit_widths(segs, [], 0.25, 0.15, 0.2)], [0.22, 0.22, 0.25])
        segs[1] = ("L", 0, 0.3, 10, 0.3, "B")
        self.assertEqual(lay1991.fit_widths(segs, [], 0.25, 0.15, 0.2)[:2], [0.15, 0.15])

    def test_a_wide_run_of_another_net_is_kept_off_by_its_half_width(self):
        segs = [("L", 0, 0, 10, 0, "A")]
        wide = [("L", 0, 0.8, 10, 0.8, 0.5, "GND")]          # 1 mm wide, 0.8 away: 0.1 a side left
        self.assertAlmostEqual(lay1991.fit_widths(segs, [], 0.25, 0.15, 0.2, fixed=wide)[0], 0.2)

    def test_a_square_pad_keeps_off_by_its_sides_not_its_corners(self):
        # a 1.6 mm square pad, its side 1.2 from the run: 0.4 - 0.2 a side leaves the full width
        segs = [("L", 0, 0, 10, 0, "A")]
        pad = (5, 2.0, 1.13, "B", 0.8, 0.8)
        self.assertEqual(lay1991.fit_widths(segs, [pad], 0.25, 0.15, 0.2), [0.25])
        pad = (5, 1.15, 1.13, "B", 0.8, 0.8)                 # side 0.35 off: 0.3 wide
        self.assertAlmostEqual(lay1991.fit_widths(segs, [pad], 0.25, 0.15, 0.2)[0], 0.25)
        pad = (5, 1.1, 1.13, "B", 0.8, 0.8)                  # side 0.3 off: 0.2 wide
        self.assertAlmostEqual(lay1991.fit_widths(segs, [pad], 0.25, 0.15, 0.2)[0], 0.2)

    def test_a_long_run_narrows_only_by_the_tight_spot(self):
        # 20 mm long, a pad 1.1 mm off its far end: the narrowing a piece long, the rest whole
        runs = lay1991.fit_runs([("L", 0, 0, 20, 0, "A")], [(19.5, 1.1, 0.8, "B")], 0.25, 0.15, 0.2, piece=2.5)
        self.assertEqual([(round(s[1], 2), round(s[3], 2), round(w, 2)) for s, w in runs],
                         [(0, 17.5, 0.25), (17.5, 20.0, 0.2)])

    def test_its_own_nets_copper_is_no_obstacle(self):
        segs = [("L", 0, 0, 10, 0, "A")]
        self.assertEqual(lay1991.fit_widths(segs, [(5, 0.5, 0.8, "A")], 0.25, 0.15, 0.2), [0.25])


if __name__ == "__main__":
    unittest.main()
