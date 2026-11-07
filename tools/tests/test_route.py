"""The routing's state between Freerouting runs: a session put back into its design."""
import os
import sys
import unittest

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
import route  # noqa: E402

DESIGN = """(pcb ns4
  (resolution um 10)
  (unit um)
  (structure
    (layer F.Cu (type signal) (property (index 0)))
    (layer In1.Cu (type signal) (property (index 1)))
    (layer In2.Cu (type power) (property (index 2)))
    (layer In3.Cu (type power) (property (index 3)))
    (layer In4.Cu (type signal) (property (index 4)))
    (layer B.Cu (type signal) (property (index 5)))
    (boundary (path pcb 0  290000 -230000  0 -230000  0 0  290000 0  290000 -230000))
    (plane GND (polygon In2.Cu 0  500 -500  289500 -500  289500 -229500  500 -229500  500 -500))
    (plane +5V (polygon In3.Cu 0  500 -500  289500 -500  289500 -229500  500 -229500  500 -500))
  )
  (network (net "ВБ1 H"))
  (wiring
    (wire (path F.Cu 250  1 -2  3 -4)(net "ВБ1 H")(type route))
  )
)
"""

SESSION = """(session ns4
  (base_design ns4)
  (routes
    (resolution um 10)
    (network_out
      (net "ВБ1 H"
        (wire
          (path In2.Cu 1874
            2461479 -1014969
            2423379 -976870
          )
        )
        (via "Via[0-5]_700:350_um" 1695783 -352866)
      )
      (net ~D94.1
        (wire (path F.Cu 2500 1603430 -241470 1877730 -253420))
        (wire (path In4.Cu 2500 1877730 -253420 1877730 -300000))
      )
    )
  )
)
"""


class Wiring(unittest.TestCase):
    def test_emptied_wiring_keeps_the_rest(self):
        out = route.without_wiring(DESIGN)
        self.assertNotIn("(wire ", out)
        self.assertIn('(network (net "ВБ1 H"))', out)
        self.assertTrue(out.rstrip().endswith(")"))

    def test_a_session_parses_with_quoted_names(self):
        tree = route.sexpr(SESSION)
        nets = route.child(route.child(tree, "routes"), "network_out")[1:]
        self.assertEqual([n[1] for n in nets], ["ВБ1 H", "~D94.1"])

    def test_session_routes_go_in_at_exact_coordinates(self):
        out = route.session_wiring(SESSION, DESIGN)
        self.assertIn('(wire (path In2.Cu 187.4 246147.9 -101496.9 242337.9 -97687)'
                      '(net "ВБ1 H")(type route))', out)
        self.assertIn('(via "Via[0-5]_700:350_um" 169578.3 -35286.6 (net "ВБ1 H")(type route))', out)
        self.assertIn('(wire (path F.Cu 250 160343 -24147 187773 -25342)(net "~D94.1")(type route))', out)
        self.assertIn('(wire (path In4.Cu 250 187773 -25342 187773 -30000)(net "~D94.1")(type route))', out)
        self.assertNotIn("1 -2  3 -4", out)          # the design's own wires are gone

    def test_a_variant_drops_the_nets_its_pins_left(self):
        out = route.session_wiring(SESSION, DESIGN, drop=["ВБ1 H"])
        self.assertNotIn('"ВБ1 H")(type', out)
        self.assertIn('(net "~D94.1")(type route))', out)

    def test_a_net_that_lost_a_pin_loses_its_wires_one_that_gained_keeps_them(self):
        old = DESIGN.replace('(network (net "ВБ1 H"))',
                             '(network (net "ВБ1 H" (pins D1-1 D80-5)) (net ~D94.1 (pins R1-1 R2-1)))')
        new = DESIGN.replace('(network (net "ВБ1 H"))',
                             '(network (net "ВБ1 H" (pins D1-1 E9-1)) (net ~D94.1 (pins R1-1 R2-1 E9-3)))')
        out, drop = route.carried(SESSION, old, new)
        self.assertEqual(drop, ["ВБ1 H"])
        self.assertNotIn('"ВБ1 H")(type', out)
        self.assertIn('(net "~D94.1")(type route))', out)

    def test_a_moved_part_refuses_the_carry(self):
        def design(x):
            return DESIGN.replace(
                '(network (net "ВБ1 H"))',
                f'(placement (component DIP (place D1 {x} -10 front 270) (place R1 0 0 front 90)))'
                '(network (net "ВБ1 H" (pins D1-1 R2-1)) (net ~D94.1 (pins R1-1 R2-2)))')
        with self.assertRaises(SystemExit) as refused:
            route.carried(SESSION, design(5), design(7))
        self.assertIn("D1", str(refused.exception))
        out, drop = route.carried(SESSION, design(5), design(5))      # as placed: carried whole
        self.assertEqual(drop, [])
        self.assertIn('(net "~D94.1")(type route))', out)

    def test_a_start_is_from_the_empty_board_unless_told(self):
        try:
            route.configure("vvv104")
            self.assertIsNone(route.carry_from([]))
            self.assertEqual(route.carry_from(["--keep"]), route.WORK)
            self.assertTrue(route.carry_from(["--from", "ns4"]).endswith(os.path.join("build", "route")))
            route.configure()
            self.assertIsNone(route.carry_from([]))
            self.assertTrue(route.carry_from(["--from", "vvv104"]).endswith(os.path.join("build", "route-vvv104")))
            with self.assertRaises(SystemExit):
                route.carry_from(["--from"])
            with self.assertRaises(SystemExit):
                route.carry_from(["--from", "ns4"])                   # its own board: that is --keep
        finally:
            route.configure()

    def test_a_variant_has_its_own_files(self):
        try:
            route.configure("vvv104")
            self.assertTrue(route.BOARD.endswith(os.path.join("kicad", "ns4-vvv104.kicad_pcb")))
            self.assertTrue(route.STATE.endswith(os.path.join("build", "route-vvv104", "state.dsn")))
        finally:
            route.configure()
        self.assertTrue(route.STATE.endswith(os.path.join("build", "route", "state.dsn")))

    def test_a_session_in_other_units_is_refused(self):
        with self.assertRaises(SystemExit):
            route.session_wiring(SESSION.replace("(resolution um 10)", "(resolution mil 10)"), DESIGN)


class Layers(unittest.TestCase):
    def test_the_design_names_its_layers_types(self):
        self.assertEqual(route.design_layers(DESIGN),
                         {"F.Cu": "signal", "In1.Cu": "signal", "In2.Cu": "power",
                          "In3.Cu": "power", "In4.Cu": "signal", "B.Cu": "signal"})

    def test_the_planes_must_come_out_as_power_layers(self):
        self.assertEqual(route.check_layers(DESIGN), "F.Cu In1.Cu In4.Cu B.Cu signal, In2.Cu In3.Cu power")
        with self.assertRaises(SystemExit) as refused:       # a plane as a signal layer: the 2026-11-05 routing
            route.check_layers(DESIGN.replace("(layer In3.Cu (type power)", "(layer In3.Cu (type signal)"))
        self.assertIn("In3.Cu", str(refused.exception))
        with self.assertRaises(SystemExit):
            route.check_layers(DESIGN.replace("(type power)", "(type signal)"))


class Options(unittest.TestCase):
    def test_freerouting_keeps_the_rules_width_and_the_boards_edge(self):
        was = os.environ.get("FREEROUTING_JAR")
        os.environ["FREEROUTING_JAR"] = __file__                  # any file: the jar is not run here
        try:
            args = route.freerouting_args("a.dsn", "b.ses", 3)
        finally:
            if was is None:
                del os.environ["FREEROUTING_JAR"]
            else:
                os.environ["FREEROUTING_JAR"] = was
        self.assertEqual(args[:3], ["java", "-jar", __file__])
        self.assertEqual(args[3:], ["-de", "a.dsn", "-do", "b.ses", "-mp", "3", "--gui.enabled=false",
                                    "--router.automatic_neckdown=false",
                                    "--router.copper_to_edge_clearance_um=500"])


if __name__ == "__main__":
    unittest.main()
