"""The routing's state between Freerouting runs: a session put back into its design."""
import os
import sys
import unittest

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
import route  # noqa: E402

DESIGN = """(pcb ns4
  (resolution um 10)
  (unit um)
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
        (via "Via[0-3]_700:350_um" 1695783 -352866)
      )
      (net ~D94.1
        (wire (path F.Cu 2500 1603430 -241470 1877730 -253420))
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
        self.assertIn('(via "Via[0-3]_700:350_um" 169578.3 -35286.6 (net "ВБ1 H")(type route))', out)
        self.assertIn('(wire (path F.Cu 250 160343 -24147 187773 -25342)(net "~D94.1")(type route))', out)
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

    def test_a_moved_part_takes_its_nets_wires_off(self):
        def design(x):
            return DESIGN.replace(
                '(network (net "ВБ1 H"))',
                f'(placement (component DIP (place D1 {x} -10 front 270) (place R1 0 0 front 90)))'
                '(network (net "ВБ1 H" (pins D1-1 R2-1)) (net ~D94.1 (pins R1-1 R2-2)))')
        out, drop = route.carried(SESSION, design(5), design(7))
        self.assertEqual(drop, ["ВБ1 H"])
        self.assertIn('(net "~D94.1")(type route))', out)

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


if __name__ == "__main__":
    unittest.main()
