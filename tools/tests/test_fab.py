"""tools/fab.py: the fab's files - which layers, which commands."""
import os
import sys
import unittest

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
import fab  # noqa: E402


class Commands(unittest.TestCase):
    def test_the_gerbers_take_all_eight_copper_layers_the_masks_the_silk_and_the_edge(self):
        for layer in ("F.Cu", "In1.Cu", "In2.Cu", "In3.Cu", "In4.Cu", "In5.Cu", "In6.Cu", "B.Cu",
                      "F.Mask", "B.Mask", "F.SilkS", "B.SilkS", "Edge.Cuts"):
            self.assertIn(layer, fab.GERBER_LAYERS)

    def test_gerbers_drill_and_the_assembly_drawing_from_one_board(self):
        cmds = fab.commands("cli", "b.kicad_pcb", "out")
        self.assertEqual([c[1:4] for c in cmds],
                         [["pcb", "export", "gerbers"], ["pcb", "export", "drill"], ["pcb", "export", "pdf"]])
        self.assertTrue(all(c[0] == "cli" and c[-1] == "b.kicad_pcb" for c in cmds))
        self.assertIn("--check-zones", cmds[0])
        self.assertIn("--excellon-separate-th", cmds[1])


if __name__ == "__main__":
    unittest.main()
