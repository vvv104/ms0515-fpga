"""Tests of tools/netlist.py: loading the netlist files and checking them."""
import os
import sys
import tempfile
import textwrap
import unittest

sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))
import netlist  # noqa: E402

PARTS = """\
ref,type,value,pins,gnd,vcc,vcc12,source,note
# a comment line
D14,К555ЛА3,,14,7,14,,ПЭ3 p23,
D83,КР1807ВМ1,,40,20,40,,ПЭ3 p24,
R62,С1-4,20 Ом,2,,,,ПЭ3 p25,
"""

PINOUTS = """\
type,pin,name,source
К555ЛА3,1,1A,74LS00
К555ЛА3,2,1B,74LS00
К555ЛА3,3,1Y,74LS00
"""

NETS = """\
net,ref,pin,pin_name,sheet,note
АД00 Н,D14,1,1A,2,
АД00 Н,D83,26,,2,
ЛН,D14,3,1Y,2,
ЛН,R62,1,,2,
"""


def write(dir_, name, text):
    path = os.path.join(dir_, name)
    with open(path, "w", encoding="utf-8") as f:
        f.write(textwrap.dedent(text))
    return path


class NetlistTest(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.dir = self.tmp.name

    def tearDown(self):
        self.tmp.cleanup()

    def load(self, parts=PARTS, pinouts=PINOUTS, nets=NETS):
        return netlist.load(write(self.dir, "parts.csv", parts),
                            write(self.dir, "pinouts.csv", pinouts),
                            write(self.dir, "nets.csv", nets))

    def errors(self, **files):
        return [p for p in netlist.check(*self.load(**files)) if p.error]

    def test_loads_parts_skipping_comments(self):
        parts, _, _ = self.load()
        self.assertEqual(set(parts), {"D14", "D83", "R62"})
        self.assertEqual(parts["D14"].type, "К555ЛА3")
        self.assertEqual(parts["D14"].power, {7: "GND", 14: "+5V"})
        self.assertEqual(parts["R62"].power, {})

    def test_power_pin_lists(self):
        parts, _, _ = self.load(parts=PARTS + "D1,КР1818ВГ93,,40,20,21,40,ПЭ3 p23,\n"
                                "D90,К554СА3А,,14,\"2,6\",11,,ПЭ3 p24,\n")
        self.assertEqual(parts["D1"].power, {20: "GND", 21: "+5V", 40: "+12V"})
        self.assertEqual(parts["D90"].power, {2: "GND", 6: "GND", 11: "+5V"})

    def test_clean_netlist_has_no_errors(self):
        self.assertEqual(self.errors(), [])

    def test_unknown_ref(self):
        errs = self.errors(nets=NETS + "X,D99,1,,2,\nX,D14,2,1B,2,\n")
        self.assertEqual(len(errs), 1)
        self.assertIn("D99", errs[0].text)

    def test_pin_out_of_range(self):
        errs = self.errors(nets=NETS + "X,D14,15,,2,\nX,D14,2,1B,2,\n")
        self.assertEqual(len(errs), 1)
        self.assertIn("D14.15", errs[0].text)

    def test_pin_in_two_nets(self):
        errs = self.errors(nets=NETS + "X,D14,1,1A,3,\nX,D14,2,1B,3,\n")
        self.assertEqual(len(errs), 1)
        self.assertIn("D14.1", errs[0].text)

    def test_pin_name_contradicts_pinout(self):
        errs = self.errors(nets=NETS + "X,D14,2,1A,2,\nX,R62,2,,2,\n")
        self.assertEqual(len(errs), 1)
        self.assertIn("1B", errs[0].text)

    def test_power_pin_in_a_signal_net(self):
        errs = self.errors(nets=NETS + "X,D14,7,,2,\nX,R62,2,,2,\n")
        self.assertEqual(len(errs), 1)
        self.assertIn("GND", errs[0].text)

    def test_power_pin_in_its_power_net_is_fine(self):
        self.assertEqual(self.errors(nets=NETS + "GND,D14,7,,1,\nGND,R62,2,,1,\n"), [])

    def test_single_pin_net_is_a_warning(self):
        problems = netlist.check(*self.load(nets=NETS + "LONE,D14,2,1B,2,\n"))
        self.assertEqual([p.error for p in problems], [False])
        self.assertIn("LONE", problems[0].text)

    def test_single_pin_net_marked_open_is_silent(self):
        problems = netlist.check(*self.load(nets=NETS + "LONE,D14,2,1B,2,open\n"))
        self.assertEqual(problems, [])

    def test_pinout_of_unknown_type_is_an_error(self):
        errs = self.errors(pinouts=PINOUTS + "К555ЛА9,1,1A,74LS03\n")
        self.assertEqual(len(errs), 1)
        self.assertIn("К555ЛА9", errs[0].text)


if __name__ == "__main__":
    unittest.main()
