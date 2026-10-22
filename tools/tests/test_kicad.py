"""The KiCad schematic: what KiCad reads out of it is the netlist."""
import os
import sys
import tempfile
import unittest

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
import kicad  # noqa: E402
import netlist as nl  # noqa: E402

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..")
NETLIST = os.path.join(ROOT, "netlist")


def ns4():
    return nl.load(*(os.path.join(NETLIST, f) for f in ("parts.csv", "pinouts.csv", "nets.csv")))


def board(parts, nets):
    d = tempfile.TemporaryDirectory()
    for name, text in (("parts.csv", "ref,type,value,pins,gnd,vcc,vcc12,source,note\n" + parts),
                       ("pinouts.csv", "type,pin,name,source\nК555ЛН1,1,A,x\nК555ЛН1,2,Y,x\n"),
                       ("nets.csv", "net,ref,pin,pin_name,sheet,note\n" + nets)):
        with open(os.path.join(d.name, name), "w", encoding="utf-8") as f:
            f.write(text)
    return d, nl.load(*(os.path.join(d.name, f) for f in ("parts.csv", "pinouts.csv", "nets.csv")))


class Items(unittest.TestCase):
    def test_a_part_goes_to_the_sheet_most_of_its_pins_were_read_from(self):
        d, loaded = board("D1,К555ЛН1,,14,7,14,,,\n",
                          "A,D1,1,,2,\nB,D1,2,,3,\nC,D1,3,,3,\n")
        with d:
            item = kicad.items(*loaded)[0]
        self.assertEqual(item.sheet, "3")
        self.assertEqual(item.footprint, "Package_DIP:DIP-14_W7.62mm")
        self.assertEqual([(p.number, p.name, p.net) for p in item.pins[:3]],
                         [(1, "A", "A"), (2, "Y", "B"), (3, "", "C")])
        self.assertEqual(item.pins[6].net, "GND")
        self.assertEqual(item.pins[4].net, "")          # on no net: a no-connect flag

    def test_the_same_netlist_gives_the_same_files(self):
        self.assertEqual(kicad.generate(*ns4()), kicad.generate(*ns4()))


class TheSchematic(unittest.TestCase):
    def test_the_committed_files_are_the_netlists(self):
        for name, text in kicad.ns4_files().items():
            with open(os.path.join(ROOT, "kicad", name), encoding="utf-8") as f:
                self.assertEqual(f.read(), text, f"kicad/{name} is stale: run tools/kicad.py")

    @unittest.skipUnless(kicad.kicad_cli(), "no kicad-cli ($KICAD_CLI or the PATH)")
    def test_kicad_reads_the_netlist_back_net_for_net(self):
        parts, pinouts, conns = ns4()
        with tempfile.TemporaryDirectory() as d:
            for name, text in kicad.generate(parts, pinouts, conns).items():
                with open(os.path.join(d, name), "w", encoding="utf-8", newline="\n") as f:
                    f.write(text)
            theirs = kicad.exported_nets(d)
        only_ours, only_kicad = kicad.differences(kicad.nets_of(parts, conns), theirs)
        self.assertEqual((only_ours, only_kicad), ([], []))
        self.assertGreater(len(theirs), 500)


if __name__ == "__main__":
    unittest.main()
