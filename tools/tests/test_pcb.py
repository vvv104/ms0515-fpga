"""The board's stack: eight copper layers as the 1991 board's, the planes handed to the router as planes."""
import math
import os
import sys
import tempfile
import unittest
from collections import Counter

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
try:
    import pcbnew
except ImportError:                     # KiCad's own Python has it; plain Python skips
    pcbnew = None
if pcbnew:
    import pcb  # noqa: E402
import route  # noqa: E402


@unittest.skipUnless(pcbnew, "no pcbnew (run with KiCad's Python)")
class Stack(unittest.TestCase):
    def test_eight_layers_the_planes_inside_as_power_layers(self):
        board = pcbnew.BOARD()
        pcb.stack(board)
        self.assertEqual(board.GetCopperLayerCount(), 8)
        self.assertEqual({board.GetLayerName(layer): kind for layer, kind in pcb.PLANES.items()},
                         {"In2.Cu": "GND", "In5.Cu": "+5V"})
        for layer in (pcbnew.F_Cu, pcbnew.In1_Cu, pcbnew.In3_Cu, pcbnew.In4_Cu, pcbnew.In6_Cu, pcbnew.B_Cu):
            self.assertEqual(board.GetLayerType(layer), pcbnew.LT_SIGNAL)
        for layer in pcb.PLANES:
            self.assertEqual(board.GetLayerType(layer), pcbnew.LT_POWER)

    def test_the_export_hands_the_planes_over_as_power_layers(self):
        board = pcbnew.BOARD()
        pcb.stack(board)
        pcb.rules(board)
        pcb.outline(board)
        codes = {}
        for layer, net in pcb.PLANES.items():
            codes[net] = pcbnew.NETINFO_ITEM(board, net)
            board.Add(codes[net])
            pcb.plane(board, codes, net, layer)
        with tempfile.TemporaryDirectory() as d:
            path = os.path.join(d, "empty.dsn")
            self.assertTrue(pcbnew.ExportSpecctraDSN(board, path))
            with open(path, encoding="utf-8") as f:
                design = f.read()
        self.assertEqual(route.design_layers(design),
                         {"F.Cu": "signal", "In1.Cu": "signal", "In2.Cu": "power", "In3.Cu": "signal",
                          "In4.Cu": "signal", "In5.Cu": "power", "In6.Cu": "signal", "B.Cu": "signal"})
        self.assertEqual(route.check_layers(design),
                         "F.Cu In1.Cu In3.Cu In4.Cu In6.Cu B.Cu signal, In2.Cu In5.Cu power")

    def test_the_edge_clearance_is_the_one_the_router_is_given(self):
        board = pcbnew.BOARD()
        pcb.rules(board)
        self.assertEqual(pcbnew.ToMM(board.GetDesignSettings().m_CopperEdgeClearance), route.EDGE_UM / 1000)


@unittest.skipUnless(pcbnew, "no pcbnew (run with KiCad's Python)")
class StandardBuild(unittest.TestCase):
    """The board as the standard build fits it (owner, 2026-11-15): what it
    leaves out has no body and is out of the lists, its pads kept; what it
    adds by hand is a body from pin to pin, no pad, the copper untouched."""

    @classmethod
    def setUpClass(cls):
        cls.board = pcbnew.LoadBoard(pcb.OUT)
        cls.pads = sum(len(fp.Pads()) for fp in cls.board.GetFootprints())
        pcb.standard_build(cls.board)
        pcb.standard_build(cls.board)       # twice: the same board

    def fp(self, ref):
        return self.board.FindFootprintByReference(ref)

    def test_the_unfitted_have_no_body_and_keep_their_pads(self):
        for ref in ("E4", "E6", "C70"):
            fp = self.fp(ref)
            self.assertTrue(fp.IsDNP() and fp.IsExcludedFromBOM() and fp.IsExcludedFromPosFiles(), ref)
            self.assertEqual(len(fp.Models()), 0, ref)
            self.assertEqual(len(fp.Pads()), 2, ref)
        self.assertFalse(self.fp("E3").IsDNP())
        self.assertEqual(len(self.fp("E3").Models()), 1)

    def test_the_hand_made_parts_run_from_pin_to_pin_without_pads(self):
        for part in pcb.hand_parts():
            fp = self.fp(part.ref)
            self.assertIsNotNone(fp, part.ref)
            self.assertEqual(len(fp.Pads()), 0)
            self.assertTrue(fp.IsBoardOnly())
            (model,) = fp.Models()
            a, b = pcb.pin_at(self.board, part.frm), pcb.pin_at(self.board, part.to)
            self.assertEqual(fp.GetPosition(), a)
            span = math.hypot(b.x - a.x, b.y - a.y)
            pitch = pcb.model_pitch(part, pcbnew.ToMM(span))
            self.assertAlmostEqual(pcbnew.ToMM(span) / model.m_Scale.x, pitch, places=3)
        self.assertEqual(sum(len(fp.Pads()) for fp in self.board.GetFootprints()), self.pads)

    def test_the_bodies_point_from_the_first_pin_to_the_second(self):
        for part in pcb.hand_parts():
            fp = self.fp(part.ref)
            a, b = pcb.pin_at(self.board, part.frm), pcb.pin_at(self.board, part.to)
            angle = math.degrees(math.atan2(-(b.y - a.y), b.x - a.x))
            self.assertAlmostEqual((fp.GetOrientationDegrees() - angle + 540) % 360 - 180, 0, places=1)


@unittest.skipUnless(pcbnew, "no pcbnew (run with KiCad's Python)")
class Placement1991(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.board = pcb.build()[0]

    def test_the_1991_outer_copper_is_laid_locked_on_its_four_layers(self):
        tracks = [t for t in self.board.GetTracks() if t.GetClass() == "PCB_TRACK"]
        vias = [t for t in self.board.GetTracks() if t.GetClass() == "PCB_VIA"]
        self.assertGreater(sum(pcbnew.ToMM(t.GetLength()) for t in tracks), 40000)   # mm: Mick's ~47 m
        self.assertGreater(len(vias), 600)
        layers = {self.board.GetLayerName(t.GetLayer()) for t in tracks}
        self.assertEqual(layers, {"F.Cu", "In1.Cu", "In6.Cu", "B.Cu"})
        self.assertTrue(all(t.IsLocked() and t.GetNetname() for t in tracks + vias))

    def test_the_1991_signal_runs_are_a_quarter_millimetre_as_the_photos_show(self):
        widths = Counter(round(pcbnew.ToMM(t.GetWidth()), 3) for t in self.board.GetTracks()
                         if t.GetClass() == "PCB_TRACK")
        self.assertEqual(widths.most_common(1)[0][0], 0.25)

    def test_only_the_supplies_are_laid_wide(self):
        wide = {t.GetNetname() for t in self.board.GetTracks()
                if t.GetClass() == "PCB_TRACK" and t.GetWidth() > pcbnew.FromMM(0.25)}
        self.assertTrue(wide)
        self.assertLessEqual(wide, {"+5V", "GND", "+12V", "-12V"})

    def test_every_part_but_the_dips_stands_on_its_1991_holes(self):
        board = self.board
        want = pcb.holes_1991()
        worst = {}
        for fp in board.GetFootprints():
            ref = fp.GetReference()
            for pad in fp.Pads():
                if pad.GetNumber() in want.get(ref, {}):
                    x, y = want[ref][pad.GetNumber()]
                    miss = math.hypot(pcbnew.ToMM(pad.GetPosition().x) - x, pcbnew.ToMM(pad.GetPosition().y) - y)
                    worst[ref] = max(worst.get(ref, 0), miss)
        self.assertEqual(sorted(worst), sorted(want))
        late = {r: round(m, 2) for r, m in worst.items() if m > pcb.MISS_OWN_GRID_MM.get(r, pcb.MISS_MM)}
        self.assertEqual(late, {})


if __name__ == "__main__":
    unittest.main()
