"""tools/inner1991.py on made-up copper: which pins must ring through the hidden layers, which must not."""
import os
import sys
import unittest

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
import inner1991  # noqa: E402

# holes 0..5; outer copper joins 0-1 (layer 1) and 2-3 (layer 8); 4 and 5 alone
LINES = {"NS4-1": [{"w": 5, "pts": [], "holes": [0, 1]}], "NS4-2": [], "NS4-7": [],
         "NS4-8": [{"w": 5, "pts": [], "holes": [2, 3]}]}


class Inner(unittest.TestCase):
    def setUp(self):
        self.group = inner1991.outer_groups(LINES, 6)

    def test_a_net_in_two_outer_pieces_rings_inside_one_in_one_does_not(self):
        pins = [("A", "D1.1", 0), ("A", "D2.3", 1), ("A", "D3.5", 4), ("B", "D4.2", 2), ("B", "D5.7", 3)]
        self.assertEqual(inner1991.inner_joins(pins, self.group), {"A": [("D3.5", "D1.1")]})

    def test_a_lifted_leg_is_not_the_pin_to_ring_to(self):
        pins = [("A", "D1.1", 0), ("A", "D2.3", 1), ("A", "D3.5", 4)]
        self.assertEqual(inner1991.inner_joins(pins, self.group, avoid=("D1.1",)), {"A": [("D3.5", "D2.3")]})

    def test_two_nets_on_one_outer_piece_must_not_ring(self):
        pins = [("A", "D1.1", 0), ("A", "D2.3", 4), ("B", "D4.2", 1)]
        self.assertEqual(inner1991.outer_shorts(pins, self.group), [("A", "D1.1", "B", "D4.2")])

    def test_a_factory_wire_joins_two_pieces_so_they_ring_through_it_not_inside(self):
        pins = [("A", "D1.1", 0), ("A", "D3.5", 4)]
        pins, group = inner1991.patched(pins, self.group, [("D1.1", "D3.5")], ())
        self.assertEqual(inner1991.inner_joins(pins, group), {})

    def test_a_lifted_leg_leaves_its_pad_s_copper_for_the_wire(self):
        # D2.3 lifted off hole 1 (on D1.1's copper) and wired to D3.5 (hole 4); D1.1 stays alone
        pins = [("A", "D1.1", 0), ("B", "D2.3", 1), ("B", "D3.5", 4)]
        pins, group = inner1991.patched(pins, self.group, [("D2.3", "D3.5")], ("D2.3",))
        self.assertEqual(inner1991.inner_joins(pins, group), {})
        self.assertEqual(inner1991.outer_shorts(pins, group), [])

    def test_a_wire_to_a_pin_not_on_the_board_is_an_error(self):
        with self.assertRaises(KeyError):
            inner1991.patched([("A", "D1.1", 0)], self.group, [("D1.1", "D9.9")], ())


if __name__ == "__main__":
    unittest.main()
