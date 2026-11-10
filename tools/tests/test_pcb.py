"""The board's stack: eight copper layers as the 1991 board's, the planes handed to the router as planes."""
import math
import os
import sys
import tempfile
import unittest

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
class Placement1991(unittest.TestCase):
    def test_every_part_but_the_dips_stands_on_its_1991_holes(self):
        board = pcb.build()[0]
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
