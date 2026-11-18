"""tools/pathfind.py on made-up boards: PathFinder on a grid whose layer changes are holes only."""
import math
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

    def test_a_plane_s_pad_on_a_piece_cut_off_the_plane_is_an_island_to_join(self):
        b = board([(2.54, 2.54, "GND", "D1.7"), (12.7, 2.54, "GND", "D2.7"), (12.7, 12.7, "GND", "R1.2")])
        self.assertNotIn("GND", pathfind.Model(b, LAYERS).islands)          # the plane joins them
        b["plane_splits"] = {"GND": [["R1.2"]]}
        islands = pathfind.Model(b, LAYERS).islands["GND"]
        self.assertEqual(len(islands), 2)
        self.assertEqual([(it[1], it[2]) for it in islands[1][1]], [(12.7, 12.7)])

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


@unittest.skipIf(pathfind is None, "no scipy")
class Smooth(unittest.TestCase):
    STAIRS = [("F.Cu", 0.0, 0.0, 1.0, 0.0, "A"), ("F.Cu", 1.0, 0.0, 1.0, 1.0, "A"),
              ("F.Cu", 1.0, 1.0, 2.0, 1.0, "A"), ("F.Cu", 2.0, 1.0, 2.0, 2.0, "A")]
    PADS = [(0.0, 0.0, "A", "D1.1", 0.3), (2.0, 2.0, "A", "D2.1", 0.3)]

    def test_a_staircase_with_nothing_in_the_way_becomes_one_line(self):
        tracks = pathfind.smooth(board(self.PADS, size=(5, 5)), self.STAIRS)
        self.assertEqual(tracks, [["F.Cu", 0.0, 0.0, 2.0, 2.0, "A"]])

    def test_a_shallow_staircase_becomes_a_diagonal_and_a_straight_run(self):
        stairs = [("F.Cu", 0.0, 0.0, 2.0, 0.0, "A"), ("F.Cu", 2.0, 0.0, 2.0, 1.0, "A"),
                  ("F.Cu", 2.0, 1.0, 4.0, 1.0, "A"), ("F.Cu", 4.0, 1.0, 4.0, 2.0, "A")]
        pads = [(0.0, 0.0, "A", "D1.1", 0.3), (4.0, 2.0, "A", "D2.1", 0.3)]
        tracks = pathfind.smooth(board(pads, size=(6, 6)), stairs)
        self.assertEqual(len(tracks), 2)
        for _, x0, y0, x1, y1, _ in tracks:
            dx, dy = abs(x1 - x0), abs(y1 - y0)
            self.assertTrue(dx < 1e-9 or dy < 1e-9 or abs(dx - dy) < 1e-9, (x0, y0, x1, y1))

    def test_another_net_s_copper_well_off_the_diagonal_is_no_obstacle(self):
        b = board(self.PADS + [(0.3, 1.7, "B", "D3.1", 0.2)], size=(5, 5))
        self.assertEqual(pathfind.smooth(b, self.STAIRS), [["F.Cu", 0.0, 0.0, 2.0, 2.0, "A"]])

    def test_another_net_s_copper_by_the_diagonal_keeps_the_corner(self):
        pads = self.PADS + [(1.3, 0.7, "B", "D3.1", 0.2)]
        tracks = pathfind.smooth(board(pads, size=(5, 5)), self.STAIRS)
        self.assertNotIn(["F.Cu", 0.0, 0.0, 2.0, 2.0, "A"], tracks)
        ends = {(t[1], t[2]) for t in tracks} | {(t[3], t[4]) for t in tracks}
        self.assertTrue({(0.0, 0.0), (2.0, 2.0)} <= ends)

    def test_another_net_s_via_by_the_diagonal_keeps_the_corner(self):
        tracks = pathfind.smooth(board(self.PADS, size=(5, 5)), self.STAIRS, [(1.3, 0.7, "B")])
        self.assertNotIn(["F.Cu", 0.0, 0.0, 2.0, 2.0, "A"], tracks)

    def test_copper_on_the_other_layer_is_no_obstacle(self):
        b = board(self.PADS, tracks=[("B.Cu", 0.0, 2.0, 2.0, 0.0, 0.25, "B")], size=(5, 5))
        self.assertEqual(pathfind.smooth(b, self.STAIRS), [["F.Cu", 0.0, 0.0, 2.0, 2.0, "A"]])

    def test_a_branch_off_a_piece_s_middle_stays_joined(self):
        branch = self.STAIRS + [("F.Cu", 1.0, 0.5, 3.0, 0.5, "A")]
        pads = self.PADS + [(3.0, 0.5, "A", "D3.1", 0.3)]
        tracks = pathfind.smooth(board(pads, size=(5, 5)), branch)
        ends = {(t[1], t[2]) for t in tracks} | {(t[3], t[4]) for t in tracks}
        self.assertIn((1.0, 0.5), ends)

    def test_a_run_through_its_net_s_pad_still_goes_through(self):
        pads = self.PADS + [(1.0, 1.0, "A", "D3.1", 0.3)]
        tracks = pathfind.smooth(board(pads, size=(5, 5)), self.STAIRS)
        self.assertLess(min(pathfind.seg_dist(1.0, 1.0, *t[1:5]) for t in tracks), 0.3)

    def test_a_pad_a_run_touched_on_the_way_it_still_touches(self):
        pads = self.PADS + [(1.0, 0.2, "A", "D3.1", 0.3)]
        tracks = pathfind.smooth(board(pads, size=(5, 5)), self.STAIRS)
        self.assertLess(min(pathfind.seg_dist(1.0, 0.2, *t[1:5]) for t in tracks), 0.3 + pathfind.WIDTH / 2)
        self.assertLess(len(tracks), len(self.STAIRS))

    def test_a_run_ending_on_a_pad_s_edge_goes_on_to_its_centre(self):
        line = [("F.Cu", 0.4, 0.0, 3.0, 0.0, "A")]
        pads = [(0.0, 0.0, "A", "D1.1", 0.5), (3.0, 0.0, "A", "D2.1", 0.3)]
        (t,) = pathfind.smooth(board(pads, size=(5, 5)), line)
        self.assertEqual(sorted([tuple(t[1:3]), tuple(t[3:5])]), [(0.0, 0.0), (3.0, 0.0)])

    def test_a_run_changing_layer_beside_a_via_goes_into_it_with_no_scrap(self):
        via = (3.2, 0.3)
        route = [("F.Cu", 0.0, 0.0, 3.0, 0.0, "A"), ("F.Cu", 3.0, 0.0, *via, "A"),
                 ("B.Cu", 3.0, 0.0, *via, "A"), ("B.Cu", 3.0, 0.0, 6.0, 0.0, "A")]
        pads = [(0.0, 0.0, "A", "D1.1", 0.3), (6.0, 0.0, "A", "D2.1", 0.3)]
        tracks = pathfind.smooth(board(pads, size=(8, 5)), route, [(*via, "A")])
        for layer, x0, y0, x1, y1, _ in tracks:
            dx, dy = abs(x1 - x0), abs(y1 - y0)
            self.assertGreater(math.hypot(dx, dy), 0.4, (layer, x0, y0, x1, y1))
            self.assertTrue(dx < 1e-9 or dy < 1e-9 or abs(dx - dy) < 1e-9, (layer, x0, y0, x1, y1))
        for layer in ("F.Cu", "B.Cu"):
            self.assertIn(via, {p for t in tracks if t[0] == layer for p in (tuple(t[1:3]), tuple(t[3:5]))})

    def test_the_corner_before_an_off_grid_end_slides_so_no_slant_is_left(self):
        copper = pathfind._Copper(board([], size=(5, 5)), [], [], ["F.Cu"])
        q = pathfind._slide(copper, 0, "A", [(0.0, 0.0), (3.0, 0.0)], (3.1, 1.0))
        self.assertAlmostEqual(q[0], 3.1)
        self.assertAlmostEqual(q[1], 0.0)
        self.assertIsNone(pathfind._slide(copper, 0, "A", [(0.0, 0.0), (3.0, 0.0)], (3.0, 1.0)))

    def test_a_corner_does_not_slide_into_another_net_s_copper(self):
        b = board([(2.6, 0.0, "B", "D3.1", 0.2)], size=(5, 5))
        copper = pathfind._Copper(b, [], [], ["F.Cu"])
        q = pathfind._slide(copper, 0, "A", [(0.0, 0.0), (2.0, 0.0)], (2.1, 1.0))
        self.assertAlmostEqual(q[0], 1.1)               # back to a diagonal, not on to the pad

    def test_an_off_grid_hole_with_no_room_to_slide_takes_the_scrap_inside_it(self):
        way = pathfind._octo_way(pathfind._Copper(board([], size=(5, 5)), [], [], ["F.Cu"]),
                                 0, "A", [(0.0, 0.0)], (3.0, 0.1), hole=True)
        self.assertEqual(len(way), 2)
        self.assertAlmostEqual(way[0][1], 0.0)          # straight on, the 45 degrees in the hole
        self.assertLess(math.dist(way[0], way[1]), 0.15)

    def test_a_run_turning_back_down_gets_no_corner_sharper_than_a_right_angle(self):
        pts = [(6.0, 0.635), (1.27, 0.635), (1.27, 0.0), (0.0, 0.0), (0.0, 3.8)]
        run = [("F.Cu", *a, *b, "A") for a, b in zip(pts, pts[1:])]
        pads = [(6.0, 0.635, "A", "D1.1", 0.3), (0.0, 3.8, "A", "D2.1", 0.3)]
        tracks = pathfind.smooth(board(pads, size=(8, 5)), run)
        ends = {}
        for t in tracks:
            for p, q in ((tuple(t[1:3]), tuple(t[3:5])), (tuple(t[3:5]), tuple(t[1:3]))):
                ends.setdefault(p, []).append(q)
        for p, qs in ends.items():
            if len(qs) == 2:
                (ax, ay), (bx, by) = qs
                self.assertLessEqual((ax - p[0]) * (bx - p[0]) + (ay - p[1]) * (by - p[1]), 1e-9, (p, qs))

    def test_a_loop_pulled_tight_lays_its_piece_once(self):
        loop = [("F.Cu", 0.0, 0.0, 1.0, 0.0, "A"), ("F.Cu", 1.0, 0.0, 1.0, 1.0, "A"),
                ("F.Cu", 1.0, 1.0, 0.0, 0.0, "A"), ("F.Cu", 0.0, 0.0, -2.0, 0.0, "A")]
        tracks = pathfind.smooth(board([(-2.0, 0.0, "A", "D1.1", 0.3)], size=(5, 5)), loop)
        keys = [frozenset([tuple(t[1:3]), tuple(t[3:5])]) for t in tracks]
        self.assertEqual(len(keys), len(set(keys)))

    def test_pieces_along_one_line_are_one(self):
        line = [("F.Cu", 0.0, 0.0, 1.0, 0.0, "A"), ("F.Cu", 1.0, 0.0, 2.0, 0.0, "A")]
        pads = [(0.0, 0.0, "A", "D1.1", 0.3), (2.0, 0.0, "A", "D2.1", 0.3), (1.0, 0.0, "A", "D3.1", 0.2)]
        self.assertEqual(pathfind.smooth(board(pads, size=(5, 5)), line), [["F.Cu", 0.0, 0.0, 2.0, 0.0, "A"]])

    def test_a_point_where_three_pieces_meet_stays(self):
        branch = self.STAIRS + [("F.Cu", 1.0, 1.0, 1.0, 3.0, "A")]
        pads = self.PADS + [(1.0, 3.0, "A", "D3.1", 0.3)]
        tracks = pathfind.smooth(board(pads, size=(5, 5)), branch)
        ends = {(t[1], t[2]) for t in tracks} | {(t[3], t[4]) for t in tracks}
        self.assertIn((1.0, 1.0), ends)


@unittest.skipIf(pathfind is None, "no scipy")
class Where(unittest.TestCase):
    def test_the_board_and_the_work_folder(self):
        board, work, rest = pathfind.where(["pathfind.py", "export"])
        self.assertTrue(board.endswith(os.path.join("kicad", "ns4.kicad_pcb")))
        self.assertTrue(work.endswith(os.path.join("build", "pathfind")))


if __name__ == "__main__":
    unittest.main()
