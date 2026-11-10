"""tools/pathfind.py on made-up boards: PathFinder on a grid whose layer changes are holes only."""
import os
import sys
import unittest

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
try:
    import scipy  # noqa: F401
    import pathfind
except ImportError:          # KiCad's Python has no scipy; the router runs on the system's
    pathfind = None

LAYERS = ["In3.Cu", "In4.Cu"]


def board(pads, free=(), tracks=(), vias=(), size=(20.32, 20.32)):
    """pads: (x, y, net, name[, radius]); radius 0.8 unless given.  The box from 0 to size."""
    return {"pads": [[p[0], p[1], p[4] if len(p) > 4 else 0.8, p[2], p[3]] for p in pads],
            "vias": [list(v) for v in vias], "tracks": [list(t) for t in tracks],
            "free": [list(f) for f in free], "box": [0.0, 0.0, size[0], size[1]]}


@unittest.skipIf(pathfind is None, "no scipy")
class Model(unittest.TestCase):
    def test_a_net_s_pads_are_its_islands_and_its_own_cells(self):
        m = pathfind.Model(board([(2.54, 2.54, "A", "D1.1"), (12.7, 2.54, "A", "D2.1")]), LAYERS)
        self.assertEqual(len(m.islands["A"]), 2)
        cell = m.node(0, *m.cell(2.54, 2.54))
        self.assertEqual(m.owner[cell], m.net_id["A"])

    def test_pads_of_one_net_joined_by_a_track_are_one_island_with_nothing_to_join(self):
        b = board([(2.54, 2.54, "A", "D1.1"), (12.7, 2.54, "A", "D2.1")],
                  tracks=[("F.Cu", 2.54, 2.54, 12.7, 2.54, 0.25, "A")])
        self.assertNotIn("A", pathfind.Model(b, LAYERS).islands)

    def test_a_run_past_a_square_pad_s_corner_reach_does_not_join_it(self):
        # a 1.6 mm square pad (corners 1.13 out, sides 0.8) and a run 1.2 mm past its centre
        b = board([(2.54, 2.54, "A", "D1.1", 1.13), (12.7, 2.54, "A", "D2.1")],
                  tracks=[("B.Cu", 3.74, 0.0, 3.74, 10.0, 0.25, "A")])
        b["pads"][0].append(0.8)
        b["pads"][1].append(0.8)
        self.assertEqual(len(pathfind.Model(b, LAYERS).islands["A"]), 2)

    def test_a_cell_between_two_nets_pads_closer_than_the_clearance_is_shut(self):
        # pads 1.2 mm round, 2.54 apart: the cell between them is too near both
        m = pathfind.Model(board([(2.54, 2.54, "A", "D1.1", 1.2), (5.08, 2.54, "B", "D1.2", 1.2)]), LAYERS)
        self.assertEqual(m.owner[m.node(0, *m.cell(3.81, 2.54))], pathfind.SHUT)


    def test_a_step_between_two_open_cells_across_another_net_s_track_is_shut(self):
        # B's track on In3 runs between the cell rows y = 2.54 and y = 3.81
        b = board([(2.54, 2.54, "A", "D1.1"), (12.7, 2.54, "A", "D2.1"), (2.54, 12.7, "B", "D3.1")],
                  tracks=[("In3.Cu", 2.54, 3.175, 12.7, 3.175, 0.25, "B")])
        m = pathfind.Model(b, ["In3.Cu"])
        i, j = m.cell(6.35, 2.54)
        self.assertTrue(m.edge_shut_for(m.node(0, i, j), m.node(0, i, j + 1), "A"))

    def test_a_square_pad_reaches_out_to_its_corners(self):
        self.assertAlmostEqual(pathfind.pad_radius(True, 2.2, 2.2, 1.2), 1.556, 3)
        self.assertEqual(pathfind.pad_radius(False, 1.5, 1.5, 1.0), 0.75)
        self.assertEqual(pathfind.pad_radius(False, 4.8, 4.8, 4.8), 2.4)

    def test_a_hole_with_no_net_keeps_the_hole_clearance(self):
        # a 4.8 mm mounting hole: copper 0.25 mm from its edge, a track's half width more
        m = pathfind.Model(board([(10.16, 10.16, "", "H1", 2.4), (2.54, 2.54, "A", "D1.1")]), LAYERS)
        i, j = m.cell(10.16 + 2.54, 10.16)        # 2.54 from the centre: 0.14 from the edge
        self.assertEqual(m.owner[m.node(0, i, j)], pathfind.SHUT)


@unittest.skipIf(pathfind is None, "no scipy")
class Route(unittest.TestCase):
    def test_two_pads_joined_on_an_inner_layer(self):
        m = pathfind.Model(board([(2.54, 2.54, "A", "D1.1"), (12.7, 2.54, "A", "D2.1")]), LAYERS)
        r = pathfind.PathFinder(m).run(10)
        self.assertEqual(r.failed, [])
        self.assertEqual(r.overused, 0)
        self.assertEqual(len(r.paths["A"]), 1)

    def test_two_nets_crossing_change_layer_in_a_free_hole_or_use_both_layers(self):
        # A runs left-right, B top-bottom; on one layer they must cross
        pads = [(2.54, 10.16, "A", "D1.1"), (17.78, 10.16, "A", "D2.1"),
                (10.16, 2.54, "B", "D3.1"), (10.16, 17.78, "B", "D4.1")]
        r = pathfind.PathFinder(pathfind.Model(board(pads), LAYERS)).run(30)
        self.assertEqual((r.failed, r.overused), ([], 0))

    def test_on_one_layer_a_crossing_needs_a_free_hole_and_finds_none(self):
        # all four pads against the box's sides: nothing passes round them
        pads = [(1.27, 10.16, "A", "D1.1"), (19.05, 10.16, "A", "D2.1"),
                (10.16, 1.27, "B", "D3.1"), (10.16, 19.05, "B", "D4.1")]
        r = pathfind.PathFinder(pathfind.Model(board(pads), ["In3.Cu"])).run(15)
        self.assertGreater(r.overused + len(r.failed), 0)

    def test_a_long_fight_never_takes_a_dear_cell_for_a_shut_one(self):
        # after many rounds the price of sharing is high; reachable islands stay reachable
        pads = [(1.27, 10.16, "A", "D1.1"), (19.05, 10.16, "A", "D2.1"),
                (10.16, 1.27, "B", "D3.1"), (10.16, 19.05, "B", "D4.1")]
        r = pathfind.PathFinder(pathfind.Model(board(pads), ["In3.Cu"])).run(80)
        self.assertEqual(r.failed, [])
        self.assertGreater(r.overused, 0)

    def test_a_net_may_not_pass_through_another_net_s_pad(self):
        # B's pad sits on A's straight line; A goes round it
        pads = [(2.54, 10.16, "A", "D1.1"), (17.78, 10.16, "A", "D2.1"), (10.16, 10.16, "B", "D3.1"),
                (10.16, 17.78, "B", "D4.1")]
        m = pathfind.Model(board(pads), ["In3.Cu"])
        r = pathfind.PathFinder(m).run(20)
        b_cell = m.node(0, *m.cell(10.16, 10.16))
        self.assertNotIn(b_cell, [n for p in r.paths["A"] for n in p])

    def test_a_layer_changes_only_in_a_hole(self):
        pads = [(2.54, 10.16, "A", "D1.1"), (17.78, 10.16, "A", "D2.1"),
                (10.16, 1.27, "B", "D3.1"), (10.16, 19.05, "B", "D4.1")]
        m = pathfind.Model(board(pads, free=[(7.62, 10.16)]), LAYERS)
        r = pathfind.PathFinder(m).run(30)
        holes = set(m.via_cells)
        for path in (p for ps in r.paths.values() for p in ps):
            for a, b in zip(path, path[1:]):
                if m.layer_of(a) != m.layer_of(b):
                    self.assertIn(a % m.per_layer, holes)


if __name__ == "__main__":
    unittest.main()
