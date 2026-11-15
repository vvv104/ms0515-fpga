import os
import sys
import tempfile
import unittest

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
import bom  # noqa: E402

PARTS = "ref,type,value,pins,gnd,vcc,vcc12,source,note\n"
PACKAGES = "type,footprint,equivalent,note\n"
CHIPS = """| Chip | Model | Analogue | Document |
|---|---|---|---|
| К555ЛА3 | `k555la3.sv` | 74LS00 | TI |
| КР580ВИ53 | `kr580vi53.sv` | Intel 8253 | Intel |
| К573РФ4Б | `k573rf4.sv` | 2764 | Intel |
"""


class Fixture:
    def __init__(self, parts, packages, firmware=""):
        self.dir = tempfile.TemporaryDirectory()
        self.paths = {}
        for name, text in (("parts.csv", PARTS + parts), ("packages.csv", PACKAGES + packages),
                           ("chips.md", CHIPS), ("firmware.csv", "ref,file,format\n" + firmware)):
            path = os.path.join(self.dir.name, name)
            with open(path, "w", encoding="utf-8") as f:
                f.write(text)
            self.paths[name] = path

    def items(self):
        return bom.items(self.paths["parts.csv"], self.paths["packages.csv"], self.paths["chips.md"],
                         self.paths["firmware.csv"])


class Chips(unittest.TestCase):
    def test_a_chip_takes_a_dip_by_its_pins_wide_from_24(self):
        self.assertEqual(bom.dip(14), "Package_DIP:DIP-14_W7.62mm")
        self.assertEqual(bom.dip(20), "Package_DIP:DIP-20_W7.62mm")
        self.assertEqual(bom.dip(24), "Package_DIP:DIP-24_W15.24mm")
        self.assertEqual(bom.dip(40), "Package_DIP:DIP-40_W15.24mm")

    def test_a_chip_takes_its_analogue_from_chips_md(self):
        items, problems = Fixture("D1,К555ЛА3,,14,7,14,,,\nD2,КР580ВИ53,,24,12,24,,,\n", "").items()
        self.assertEqual(problems, [])
        self.assertEqual(items["D1"].footprint, "Package_DIP:DIP-14_W7.62mm")
        self.assertEqual(items["D1"].equivalent, "74LS00")
        self.assertEqual(items["D2"].footprint, "Package_DIP:DIP-24_W15.24mm")

    def test_a_chip_in_a_socket_is_not_placed_itself(self):
        items, problems = Fixture("D65,К573РФ4Б,,28,14,28,,,\nXS3,РС-28-7,,28,,,,,\n",
                                  "РС-28-7,Package_DIP:DIP-28_W15.24mm_Socket,socket,\n").items()
        self.assertEqual(problems, [])
        self.assertFalse(items["D65"].placed)
        self.assertTrue(items["XS3"].placed)
        self.assertIn("XS3", items["D65"].note)

    def test_a_programmed_chip_names_its_image_and_lines_apart_by_it(self):
        items, problems = Fixture("D65,К573РФ4Б,,28,14,28,,,\nD77,К573РФ4Б,,28,14,28,,,\n"
                                  "XS3,РС-28-7,,28,,,,,\nXS4,РС-28-7,,28,,,,,\n",
                                  "РС-28-7,Package_DIP:DIP-28_W15.24mm_Socket,socket,\n",
                                  "D65,firmware/romb.rom,even-bytes\nD77,firmware/romb.rom,odd-bytes\n").items()
        self.assertEqual(problems, [])
        self.assertIn("firmware/romb.rom, even-bytes", items["D65"].note)
        self.assertIn("XS3", items["D65"].note)
        roms = [l for l in bom.lines(items) if l.type == "К573РФ4Б"]
        self.assertEqual([l.refs for l in roms], [["D65"], ["D77"]])


class Others(unittest.TestCase):
    def test_a_part_takes_its_type_s_footprint(self):
        items, problems = Fixture("R1,С1-4,1 кОм,2,,,,,\n",
                                  "С1-4,Resistor_THT:R_Axial,0.25 W film,\n").items()
        self.assertEqual(problems, [])
        self.assertEqual(items["R1"].footprint, "Resistor_THT:R_Axial")
        self.assertEqual(items["R1"].equivalent, "0.25 W film")
        self.assertEqual(items["R1"].value, "1 кОм")

    def test_a_type_without_a_footprint_is_an_error(self):
        _, problems = Fixture("R1,С1-4,1 кОм,2,,,,,\n", "").items()
        self.assertTrue(any("R1" in p and "С1-4" in p for p in problems))

    def test_a_chip_without_an_analogue_is_an_error(self):
        _, problems = Fixture("D1,К555ЛА9,,14,7,14,,,\n", "").items()
        self.assertTrue(any("К555ЛА9" in p for p in problems))


class Table(unittest.TestCase):
    def test_parts_of_one_type_and_value_are_one_line(self):
        items, _ = Fixture("R2,С1-4,1 кОм,2,,,,,\nR1,С1-4,1 кОм,2,,,,,\nR3,С1-4,2 кОм,2,,,,,\n",
                           "С1-4,Resistor_THT:R_Axial,film,\n").items()
        lines = bom.lines(items)
        self.assertEqual([(l.type, l.value, l.refs) for l in lines],
                         [("С1-4", "1 кОм", ["R1", "R2"]), ("С1-4", "2 кОм", ["R3"])])

    def test_the_table_counts_and_names_them(self):
        items, _ = Fixture("R1,С1-4,1 кОм,2,,,,,\nR2,С1-4,1 кОм,2,,,,,\n",
                           "С1-4,Resistor_THT:R_Axial,film,\n").items()
        text = bom.markdown(bom.lines(items))
        self.assertIn("| С1-4 | 1 кОм | 2 | R1, R2 | film | `Resistor_THT:R_Axial` |  |", text)


class StandardBuild(unittest.TestCase):
    """What the standard build leaves out and what it adds by hand."""

    def write(self, name, text):
        self.dir = getattr(self, "dir", None) or tempfile.TemporaryDirectory()
        path = os.path.join(self.dir.name, name)
        with open(path, "w", encoding="utf-8") as f:
            f.write(text)
        return path

    def test_a_jumper_left_open_is_not_fitted_and_a_fitted_one_is(self):
        analog = self.write("analog.csv", "ref,treatment,pins,reason\n"
                            "E1,short,,fitted\nE3,jumper,fitted,x\nE4,jumper,open,x\nE6,open,,x\n"
                            "R38,open,,video output load\n")
        self.assertEqual(bom.not_fitted(analog, {"C70"}), {"E4", "E6", "C70"})

    def test_the_hand_made_parts_are_read_with_their_pins(self):
        path = self.write("assembly_parts.csv", "# x\nref,type,value,from,to,lifts,evidence\n"
                          "R81,С1-4,620 кОм,R38.1,VT1.14,VT1.14,photos\n")
        (part,) = bom.assembly_parts(path)
        self.assertEqual((part.ref, part.value, part.frm, part.to, part.lifts),
                         ("R81", "620 кОм", ("R38", 1), ("VT1", 14), ("VT1", 14)))

    def test_the_list_leaves_the_unfitted_out_and_names_them_apart(self):
        items, _ = Fixture("C5,К10-17,100 пФ,2,,,,,\nC70,К10-17,100 пФ,2,,,,,\n",
                           "К10-17,Capacitor_THT:C_Disc,ceramic,\n").items()
        parts = bom.assembly_parts(self.write("a.csv", "ref,type,value,from,to,lifts,evidence\n"
                                              "W1,провод,,D70.15,D57.10,D70.15,photo\n"))
        text = bom.document(items, {"C70"}, parts)
        self.assertIn("| К10-17 | 100 пФ | 1 | C5 |", text)
        self.assertIn("Not fitted: C70", text)
        self.assertIn("| W1 | провод |  | D70.15 | D57.10 | D70.15 |", text)


if __name__ == "__main__":
    unittest.main()
