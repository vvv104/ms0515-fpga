"""The board's stack: six copper layers, the planes handed to the router as planes."""
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
    def test_six_layers_the_planes_inside_as_power_layers(self):
        board = pcbnew.BOARD()
        pcb.stack(board)
        self.assertEqual(board.GetCopperLayerCount(), 6)
        self.assertEqual({board.GetLayerName(layer): kind for layer, kind in pcb.PLANES.items()},
                         {"In2.Cu": "GND", "In3.Cu": "+5V"})
        for layer in (pcbnew.F_Cu, pcbnew.In1_Cu, pcbnew.In4_Cu, pcbnew.B_Cu):
            self.assertEqual(board.GetLayerType(layer), pcbnew.LT_SIGNAL)
        for layer in pcb.PLANES:
            self.assertEqual(board.GetLayerType(layer), pcbnew.LT_POWER)

    def test_the_export_hands_the_planes_over_as_power_layers(self):
        board = pcbnew.BOARD()
        pcb.stack(board)
        pcb.rules(board)
        pcb.outline(board)
        with tempfile.TemporaryDirectory() as d:
            path = os.path.join(d, "empty.dsn")
            self.assertTrue(pcbnew.ExportSpecctraDSN(board, path))
            with open(path, encoding="utf-8") as f:
                design = f.read()
        self.assertEqual(route.design_layers(design),
                         {"F.Cu": "signal", "In1.Cu": "signal", "In2.Cu": "power",
                          "In3.Cu": "power", "In4.Cu": "signal", "B.Cu": "signal"})
        self.assertEqual(route.check_layers(design), "F.Cu In1.Cu In4.Cu B.Cu signal, In2.Cu In3.Cu power")

    def test_the_edge_clearance_is_the_one_the_router_is_given(self):
        board = pcbnew.BOARD()
        pcb.rules(board)
        self.assertEqual(pcbnew.ToMM(board.GetDesignSettings().m_CopperEdgeClearance), route.EDGE_UM / 1000)


if __name__ == "__main__":
    unittest.main()
