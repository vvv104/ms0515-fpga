"""The memory expansion module's printed board made anew (plan 6a step 5):
the original's outline and placement (its photos), its plug where НС4's XS1
takes it, four layers with the supplies' planes inside, its pads on the
module's netlist."""
import json
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
    import pcb_ext  # noqa: E402


def mm(v):
    return pcbnew.ToMM(v)


@unittest.skipUnless(pcbnew, "no pcbnew (run with KiCad's Python)")
class Outline(unittest.TestCase):
    def test_the_outline_is_the_originals_less_the_notch(self):
        pts = pcb_ext.OUTLINE
        xs, ys = [p[0] for p in pts], [p[1] for p in pts]
        self.assertEqual((min(xs), max(xs), min(ys), max(ys)), (0, 246.1, 0, 71.1))
        area = abs(sum(x0 * y1 - x1 * y0 for (x0, y0), (x1, y1) in zip(pts, pts[1:] + pts[:1]))) / 2
        cut = (pcb_ext.NOTCH[1] - pcb_ext.NOTCH[0]) * pcb_ext.NOTCH_DEPTH
        self.assertAlmostEqual(area, 246.1 * 71.1 - cut)

    def test_one_notch_over_sa1_left_of_the_plug(self):
        """The original's only notch (owner, 2026-11-17): over НС4's
        switches, left of the plug, whose face is above the lower edge."""
        self.assertEqual(list(pcb_ext.CUTS), ["SA1"])
        self.assertLess(pcb_ext.NOTCH[1], pcb_ext.A1_X - 8.18)        # the plug's body begins 8.18 left of a1
        face = pcb_ext.ROW_A_Y + pcb_ext.ROW_A_TO_FACE
        self.assertAlmostEqual(pcb_ext.BOARD_H - face, 8.95, delta=0.1)    # a DIN 41612 C female's height


@unittest.skipUnless(pcbnew, "no pcbnew (run with KiCad's Python)")
class Board(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.board, cls.skipped, cls.missing, cls.failed = pcb_ext.build()

    def fp(self, ref):
        return self.board.FindFootprintByReference(ref)

    def test_every_part_has_its_footprint_and_place(self):
        self.assertEqual((self.skipped, self.missing, self.failed), ([], [], []))

    def test_four_layers_the_supplies_on_the_inner_two(self):
        self.assertEqual(self.board.GetCopperLayerCount(), 4)
        planes = {z.GetLayer(): z.GetNetname() for z in self.board.Zones()}
        self.assertEqual(planes, {pcbnew.In1_Cu: "GND", pcbnew.In2_Cu: "+5V"})
        for layer in planes:
            self.assertEqual(self.board.GetLayerType(layer), pcbnew.LT_POWER)
        for layer in (pcbnew.F_Cu, pcbnew.B_Cu):
            self.assertEqual(self.board.GetLayerType(layer), pcbnew.LT_SIGNAL)

    def test_every_part_of_the_photos_on_its_holes(self):
        for ref, (x, y, rotation) in pcb_ext.placement().items():
            one = self.fp(ref).FindPadByNumber("1").GetPosition()
            self.assertAlmostEqual(mm(one.x), x, delta=0.01, msg=ref)
            self.assertAlmostEqual(mm(one.y), y, delta=0.01, msg=ref)

    def test_two_tracks_pass_between_two_pins_of_a_dip(self):
        nc = self.board.GetDesignSettings().m_NetSettings.GetDefaultNetclass()
        width, gap = mm(nc.GetTrackWidth()), mm(nc.GetClearance())
        pad = 1.6                                          # Package_DIP's round pad
        self.assertLessEqual(2 * width + 3 * gap, 2.54 - pad)
        self.assertGreaterEqual(gap, 0.15)                 # a fab's common 6 mil
        self.assertLessEqual(mm(nc.GetViaDiameter()), 0.6)
        self.assertGreaterEqual(mm(nc.GetViaDrill()), 0.3)

    def test_the_12v_supplies_run_narrower_than_the_5v(self):
        # read back from the project KiCad saves beside a board with the
        # rules alone: in one run with the other tests pcbnew's net settings
        # come back as bare SWIG pointers, and saving the board itself would
        # join its tracks' segments under the other tests
        rules = pcbnew.BOARD()
        pcb_ext.rules(rules)
        with tempfile.TemporaryDirectory() as tmp:
            rules.Save(os.path.join(tmp, "ext.kicad_pcb"))
            with open(os.path.join(tmp, "ext.kicad_pro"), encoding="utf-8") as f:
                net = json.load(f)["net_settings"]
        widths = {c["name"]: c["track_width"] for c in net["classes"]}
        assigned = {p["pattern"]: p["netclass"] for p in net["netclass_patterns"]}
        width = {n: widths[assigned[n]] for n in ("+5V", "GND", "+12V", "-12V")}
        self.assertEqual((width["+5V"], width["GND"]), (0.5, 0.5))
        self.assertEqual((width["+12V"], width["-12V"]), (0.3, 0.3))

    def test_the_plug_on_the_solder_side_on_the_originals_holes(self):
        fp = self.fp("XP1")
        self.assertTrue(fp.IsFlipped())
        pads = {p.GetNumber(): p.GetPosition() for p in fp.Pads() if p.GetNumber()}
        a1, a32 = pads["a1"], pads["a32"]
        self.assertAlmostEqual(mm(a1.x), pcb_ext.A1_X, places=2)
        self.assertAlmostEqual(mm(a32.x) - mm(a1.x), 31 * 2.54, places=2)
        self.assertAlmostEqual(mm(a1.y), pcb_ext.ROW_A_Y, places=2)
        self.assertAlmostEqual(mm(pads["c1"].y), pcb_ext.ROW_A_Y - 5.08, places=2)

    def test_the_board_comes_with_no_copper_the_router_lays_it_all(self):
        """The photos' copper drawn anew (owner, 2026-11-18): their holes stay
        free for the router's vias (netlist/ext/holes.csv), no track is laid."""
        self.assertEqual(list(self.board.GetTracks()), [])

    def test_the_plug_has_no_mounting_holes(self):
        """Where Harting's footprint drills them the original runs tracks
        (owner, 2026-11-17): the plug is held by its 96 pins."""
        self.assertEqual([p for p in self.fp("XP1").Pads() if not p.GetNumber()], [])

    def test_the_two_contacts_on_ns4s_ground_are_left_free(self):
        nets = {p.GetNumber(): p.GetNetname() for p in self.fp("XP1").Pads()}
        for pad in ("c19", "c20"):
            self.assertFalse(nets[pad] and not nets[pad].startswith("~"), nets[pad])

    def test_the_board_carries_the_ram_disk_alone(self):
        """The parts on the owner's module (netlist/ext/assembly.csv) and the
        mounting holes - not the RS-232 half, not the ROM board's socket."""
        refs = {fp.GetReference() for fp in self.board.GetFootprints()}
        want = {pcb_ext.SOCKETS.get(r, r) for r in pcb_ext.assembly()}
        self.assertEqual(refs, want | set(pcb_ext.HOLES))

    def test_its_chips_are_those_fitted_csv_marks_fitted(self):
        fitted = {r["ref"] for r in pcb_ext.nl.rows(os.path.join(pcb_ext.EXT, "fitted.csv")) if r["fitted"] == "yes"}
        self.assertEqual({r for r in pcb_ext.assembly() if r.startswith(("D", "G"))}, fitted)

    def test_vvv55_stands_in_its_socket(self):
        self.assertIsNone(self.fp("D2"))
        self.assertIsNotNone(self.fp("XS3"))

    def test_no_two_parts_pads_touch(self):
        """The original packs its parts closer than KiCad's courtyards allow
        (its capacitors against the RAM, ВГ01's socket 2 mm from КП2's
        pins): what must hold is that no two parts' pads come within the
        signals' clearance of each other."""
        gap = mm(self.board.GetDesignSettings().m_NetSettings.GetDefaultNetclass().GetClearance())
        pads = [(fp.GetReference(), p) for fp in self.board.GetFootprints() for p in fp.Pads()
                if p.GetAttribute() == pcbnew.PAD_ATTRIB_PTH]
        hits = []
        for i, (a, p) in enumerate(pads):
            for b, q in pads[i + 1:]:
                if a == b:
                    continue
                d = math.hypot(mm(p.GetPosition().x - q.GetPosition().x), mm(p.GetPosition().y - q.GetPosition().y))
                room = (mm(p.GetSize(pcbnew.F_Cu).x) + mm(q.GetSize(pcbnew.F_Cu).x)) / 2 + gap
                if d < room:
                    hits.append(f"{a}.{p.GetNumber()} {b}.{q.GetNumber()} {d:.2f} mm")
        self.assertEqual(hits, [])

    def test_every_pad_on_the_board(self):
        for fp in self.board.GetFootprints():
            for p in fp.Pads():
                at, r = p.GetPosition(), mm(p.GetSize(pcbnew.F_Cu).x) / 2
                x, y = mm(at.x), mm(at.y)
                self.assertTrue(pcb_ext.inside((x - r, y - r, x + r, y + r)), f"{fp.GetReference()}.{p.GetNumber()}")

    def test_no_pad_of_the_parts_side_on_a_pad_of_the_plug(self):
        plug = [p for p in self.fp("XP1").Pads()]
        for fp in self.board.GetFootprints():
            if fp.IsFlipped():
                continue
            for p in fp.Pads():
                for q in plug:
                    d = math.hypot(mm(p.GetPosition().x - q.GetPosition().x), mm(p.GetPosition().y - q.GetPosition().y))
                    self.assertGreater(d, 2.0, f"{fp.GetReference()}.{p.GetNumber()} on XP1.{q.GetNumber()}")

    def test_the_pads_are_the_netlists(self):
        self.assertEqual(pcb_ext.check(self.board), [])


if __name__ == "__main__":
    unittest.main()
