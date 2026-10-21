"""The board generator, on small boards made up for each case."""
import os
import sys
import tempfile
import unittest

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
import board  # noqa: E402

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..")

INVERTER = """// К555ЛН1 (74LS04): six inverters.
module k555ln1 (
    input  logic p1, output logic p2,
    input  logic p3, output logic p4
);
    assign p2 = ~p1;
    assign p4 = ~p3;
endmodule
"""
OC_INVERTER = """// К555ЛН2 (74LS05): six inverters, open-collector outputs.
module k555ln2 (
    input  logic p1, output logic p2
);
    assign p2 = ~p1;
endmodule
"""
BUFFER = """// К555АП5 (74LS244): buffers, three-state.
module k555ap5 (
    input  logic p1, input  logic p2,
    output logic p18, output logic p18_oe
);
    assign p18 = p2;
    assign p18_oe = ~p1;
endmodule
"""
FLOP = """// К555ТМ2 (74LS74): a D flip-flop.
module k555tm2 (
    input  logic clk,
    input  logic p2, input  logic p3,
    output logic p5
);
    logic q;
    always_ff @(posedge clk) if (p3) q <= p2;
    assign p5 = q;
endmodule
"""


TRANSCEIVER = """// A made-up chip with one pin both driven and read.
module k555ap6 (
    input  logic p1, input  logic p2_in,
    output logic p2, output logic p2_oe, output logic p3
);
    assign p2 = 1'b0;
    assign p2_oe = p1;
    assign p3 = p2_in;
endmodule
"""


class Fixture:
    """A board in a temporary folder: parts, nets, treatments and chip modules."""

    def __init__(self, parts, nets, analog="", modules=(INVERTER,)):
        self.dir = tempfile.TemporaryDirectory()
        d = self.dir.name
        os.makedirs(os.path.join(d, "chips"))
        self.write("parts.csv", "ref,type,value,pins,gnd,vcc,vcc12,source,note\n" + parts)
        self.write("pinouts.csv", "type,pin,name,source\n")
        self.write("nets.csv", "net,ref,pin,pin_name,sheet,note\n" + nets)
        self.write("analog.csv", "ref,treatment,pins,reason\n" + analog)
        for text in modules:
            name = text.split("module ")[1].split(" ")[0]
            self.write(os.path.join("chips", name + ".sv"), text)

    def write(self, name, text):
        with open(os.path.join(self.dir.name, name), "w", encoding="utf-8") as f:
            f.write(text)

    def generate(self):
        with self.dir as d:
            return board.generate(board.Sources(
                parts=os.path.join(d, "parts.csv"), pinouts=os.path.join(d, "pinouts.csv"),
                nets=os.path.join(d, "nets.csv"), changes=None,
                analog=os.path.join(d, "analog.csv"), chips=os.path.join(d, "chips")))


def errors(problems):
    return [p.text for p in problems if p.error]


class Firmware(unittest.TestCase):
    def test_nibbles_are_words_from_the_low_bits_up(self):
        self.assertEqual(board.contents(bytes([0x01, 0xF2, 0x03]), "nibbles"), "12'h321")

    def test_bytes(self):
        self.assertEqual(board.contents(bytes([0x01, 0xAB]), "bytes"), "16'hab01")

    def test_the_halves_of_a_word_image(self):
        image = bytes([0x01, 0x02, 0x03, 0x04])             # words 0x0201, 0x0403
        self.assertEqual(board.contents(image, "even-bytes"), "16'h0301")
        self.assertEqual(board.contents(image, "odd-bytes"), "16'h0402")

    def test_a_chip_with_an_image_gets_it_as_its_contents(self):
        f = Fixture("D1,К555ЛН1,,14,7,14,,,\n", "A,D1,1,,1,\nB,D1,2,,1,\n")
        f.write("image.bin", "")
        with open(os.path.join(f.dir.name, "image.bin"), "wb") as out:
            out.write(bytes([0x0A, 0x05]))
        os.makedirs(os.path.join(f.dir.name, "netlist"))
        f.write(os.path.join("netlist", "firmware.csv"), "ref,file,format\nD1,image.bin,nibbles\n")
        with f.dir as d:
            text, _ = board.generate(board.Sources(
                parts=os.path.join(d, "parts.csv"), pinouts=os.path.join(d, "pinouts.csv"),
                nets=os.path.join(d, "nets.csv"), changes=None,
                analog=os.path.join(d, "analog.csv"), chips=os.path.join(d, "chips"),
                firmware=os.path.join(d, "netlist", "firmware.csv")))
        self.assertIn("k555ln1 #(.CONTENTS(8'h5a)) D1 (", text)


class Names(unittest.TestCase):
    def test_identifiers_are_transliterated(self):
        self.assertEqual(board.ident("ТС8М L"), "n_TS8M_L")
        self.assertEqual(board.ident("~D15.13"), "nx_D15_13")
        self.assertEqual(board.ident("СТЫК С2 103"), "n_STYK_S2_103")
        self.assertEqual(board.ident("ЧТМАГ H"), "n_CHTMAG_H")

    def test_module_name_drops_the_grade_letter(self):
        self.assertEqual(board.module_name("К555ЛА3"), "k555la3")
        self.assertEqual(board.module_name("К554СА3А"), "k554sa3")
        self.assertEqual(board.module_name("КР580ВВ55А"), "kr580vv55")
        self.assertEqual(board.module_name("КР1807ВМ1"), "kr1807vm1")

    def test_chip_types(self):
        for t in ("К555ЛА3", "КР1102АП15", "КР565РУ5Г", "К573РФ4Б"):
            self.assertTrue(board.is_chip(t), t)
        for t in ("К10-17-2б-Н90", "К53-14", "КД522Б", "КС133А", "КТ676АС", "С1-4", "КМ-6Б-Н90"):
            self.assertFalse(board.is_chip(t), t)


class Ports(unittest.TestCase):
    def test_ports_and_open_collector(self):
        with tempfile.TemporaryDirectory() as d:
            for text in (INVERTER, OC_INVERTER):
                name = text.split("module ")[1].split(" ")[0]
                with open(os.path.join(d, name + ".sv"), "w", encoding="utf-8") as f:
                    f.write(text)
            modules = board.load_modules(d)
        self.assertEqual(modules["k555ln1"].ports, {"p1": "input", "p2": "output",
                                                     "p3": "input", "p4": "output"})
        self.assertFalse(modules["k555ln1"].open_collector)
        self.assertTrue(modules["k555ln2"].open_collector)


class Generate(unittest.TestCase):
    def test_a_chain_of_inverters(self):
        f = Fixture("D1,К555ЛН1,,14,7,14,,,\n",
                    "A,D1,1,,1,\nB,D1,2,,1,\nB,D1,3,,1,\nC,D1,4,,1,\n")
        text, problems = f.generate()
        self.assertEqual(errors(problems), [])
        self.assertIn("k555ln1 D1 (", text)
        self.assertIn(".p1(n_A)", text)
        self.assertIn(".p2(d_D1_p2)", text)
        self.assertIn("assign n_B = d_D1_p2;", text)
        self.assertIn("output logic n_C", text)

    def test_an_input_nobody_drives_reads_high_and_is_reported(self):
        f = Fixture("D1,К555ЛН1,,14,7,14,,,\n", "A,D1,1,,1,\nB,D1,2,,1,\n")
        text, problems = f.generate()
        self.assertIn("assign n_A = 1'b1;", text)
        self.assertTrue(any("'A'" in p.text and "nothing drives" in p.text for p in problems))

    def test_an_unconnected_input_is_tied_high_and_an_output_gets_its_own_net(self):
        f = Fixture("D1,К555ЛН1,,14,7,14,,,\n", "A,D1,1,,1,\nB,D1,2,,1,\n")
        text, _ = f.generate()
        self.assertIn(".p3(1'b1)", text)
        self.assertIn("output logic nx_D1_4", text)

    def test_power_nets_are_constants(self):
        f = Fixture("D1,К555ЛН1,,14,7,14,,,\n", "GND,D1,1,,1,\nB,D1,2,,1,\n+5V,D1,3,,1,\n")
        text, problems = f.generate()
        self.assertEqual(errors(problems), [])
        self.assertIn(".p1(1'b0)", text)
        self.assertIn(".p3(1'b1)", text)

    def test_two_outputs_on_one_net_is_an_error(self):
        f = Fixture("D1,К555ЛН1,,14,7,14,,,\n", "B,D1,2,,1,\nB,D1,4,,1,\n")
        _, problems = f.generate()
        self.assertTrue(any("two outputs" in e for e in errors(problems)))

    def test_open_collectors_and_with_their_pull_up(self):
        f = Fixture("D1,К555ЛН2,,14,7,14,,,\nD2,К555ЛН2,,14,7,14,,,\nR1,С1-4,1 кОм,2,,,,,\n",
                    "A,D1,1,,1,\nB,D2,1,,1,\nW,D1,2,,1,\nW,D2,2,,1,\nW,R1,1,,1,\n+5V,R1,2,,1,\n",
                    modules=(OC_INVERTER,))
        text, problems = f.generate()
        self.assertEqual(errors(problems), [])
        self.assertIn("assign n_W = d_D1_p2 & d_D2_p2;", text)

    def test_three_state_drivers_resolve_with_the_pull_and_flag_contention(self):
        f = Fixture("D1,К555АП5,,20,10,20,,,\nD2,К555АП5,,20,10,20,,,\nR1,С1-4,1 кОм,2,,,,,\n",
                    "E1,D1,1,,1,\nE2,D2,1,,1,\nA,D1,2,,1,\nA,D2,2,,1,\n"
                    "Y,D1,18,,1,\nY,D2,18,,1,\nY,R1,1,,1,\n+5V,R1,2,,1,\n",
                    modules=(BUFFER,))
        text, problems = f.generate()
        self.assertEqual(errors(problems), [])
        self.assertIn("assign n_Y = (d_D1_p18_oe | d_D2_p18_oe) ? "
                      "((d_D1_p18_oe & d_D1_p18) | (d_D2_p18_oe & d_D2_p18)) : 1'b1;", text)
        self.assertIn("assign contention_at[0] = $countones({d_D1_p18_oe, d_D2_p18_oe}) > 1;", text)

    def test_a_chip_with_no_module_is_brought_out(self):
        f = Fixture("D1,К555ЛН1,,14,7,14,,,\nD9,КР1807ВМ1,,40,20,40,,,\n",
                    "A,D9,5,,1,\nA,D1,1,,1,\nB,D1,2,,1,\nB,D9,6,,1,\n")
        text, problems = f.generate()
        self.assertEqual(errors(problems), [])
        self.assertIn("input  logic x_D9_p5", text)
        self.assertIn("input  logic x_D9_p5_oe", text)
        self.assertIn("assign n_A = x_D9_p5_oe ? x_D9_p5 : 1'b1;", text)
        self.assertIn("assign n_B = d_D1_p2;", text)
        self.assertNotIn("x_D9_p6", text)

    def test_a_pin_driven_and_read_is_both_a_driver_and_a_reader_of_its_net(self):
        f = Fixture("D1,К555АП6,,20,10,20,,,\n", "E,D1,1,,1,\nB,D1,2,,1,\nC,D1,3,,1,\n",
                    modules=(TRANSCEIVER,))
        text, problems = f.generate()
        self.assertEqual(errors(problems), [])
        self.assertIn(".p2_in(n_B)", text)
        self.assertIn(".p2(d_D1_p2)", text)
        self.assertIn("assign n_B = d_D1_p2_oe ? d_D1_p2 : 1'b1;", text)

    def test_a_three_state_output_on_an_output_s_net_fights_where_they_differ(self):
        f = Fixture("D1,К555ЛН1,,14,7,14,,,\nD2,К555АП5,,20,10,20,,,\n",
                    "A,D1,1,,1,\nB,D1,2,,1,\nE,D2,1,,1,\nA,D2,2,,1,\nB,D2,18,,1,\n",
                    modules=(INVERTER, BUFFER))
        text, problems = f.generate()
        self.assertEqual(errors(problems), [])
        self.assertIn("assign n_B = d_D1_p2;", text)
        self.assertIn("assign contention_at[0] = (d_D2_p18_oe & (d_D2_p18 ^ d_D1_p2));", text)

    def test_clock_goes_to_stateful_chips(self):
        f = Fixture("D1,К555ТМ2,,14,7,14,,,\n", "A,D1,2,,1,\nC,D1,3,,1,\nQ,D1,5,,1,\n",
                    modules=(FLOP,))
        text, problems = f.generate()
        self.assertEqual(errors(problems), [])
        self.assertIn(".clk(clk)", text)

    def test_on_a_clocked_board_the_brought_out_pins_come_in_through_registers(self):
        f = Fixture("D1,К555ТМ2,,14,7,14,,,\nD9,КР1807ВМ1,,40,20,40,,,\n",
                    "A,D9,5,,1,\nA,D1,2,,1,\nC,D1,3,,1,\nQ,D1,5,,1,\n",
                    modules=(FLOP,))
        text, problems = f.generate()
        self.assertEqual(errors(problems), [])
        self.assertIn("input  logic x_D9_p5", text)
        self.assertIn("    logic q_x_D9_p5;", text)
        self.assertIn("        q_x_D9_p5 <= x_D9_p5;", text)
        self.assertIn("        q_x_D9_p5_oe <= x_D9_p5_oe;", text)
        self.assertIn("assign n_A = q_x_D9_p5_oe ? q_x_D9_p5 : 1'b1;", text)

    def test_a_passive_without_a_treatment_is_an_error(self):
        f = Fixture("D1,К555ЛН1,,14,7,14,,,\nC1,К10-17-1б-М47,68 пФ,2,,,,,\n",
                    "A,D1,1,,1,\nB,D1,2,,1,\nB,C1,1,,1,\nGND,C1,2,,1,\n")
        _, problems = f.generate()
        self.assertTrue(any("C1" in e and "analog.csv" in e for e in errors(problems)))

    def test_decoupling_needs_no_treatment(self):
        f = Fixture("D1,К555ЛН1,,14,7,14,,,\nC1,К10-17-2б-Н90,,2,,,,,\n",
                    "A,D1,1,,1,\nB,D1,2,,1,\n+5V,C1,1,,1,\nGND,C1,2,,1,\n")
        _, problems = f.generate()
        self.assertEqual(errors(problems), [])

    def test_a_short_joins_two_nets(self):
        f = Fixture("D1,К555ЛН1,,14,7,14,,,\nE1,перемычка,,2,,,,,\n",
                    "A,D1,1,,1,\nB,D1,2,,1,\nB,E1,1,,1,\nC,E1,2,,1,\nC,D1,3,,1,\n",
                    analog="E1,short,1-2,fitted\n")
        text, problems = f.generate()
        self.assertEqual(errors(problems), [])
        self.assertIn(".p3(n_B)", text)
        self.assertNotIn("n_C", text)

    def test_a_short_to_ground_makes_the_net_a_constant(self):
        f = Fixture("D1,К555ЛН1,,14,7,14,,,\nE1,перемычка,,2,,,,,\n",
                    "A,D1,1,,1,\nA,E1,1,,1,\nGND,E1,2,,1,\nB,D1,2,,1,\n",
                    analog="E1,short,1-2,fitted\n")
        text, problems = f.generate()
        self.assertEqual(errors(problems), [])
        self.assertIn(".p1(1'b0)", text)

    def test_an_open_part_is_left_out(self):
        f = Fixture("D1,К555ЛН1,,14,7,14,,,\nE1,перемычка,,2,,,,,\n",
                    "A,D1,1,,1,\nA,E1,1,,1,\nGND,E1,2,,1,\nB,D1,2,,1,\n",
                    analog="E1,open,,not fitted\n")
        text, problems = f.generate()
        self.assertIn("assign n_A = 1'b1;", text)

    def test_a_connector_brings_its_signal_pins_out(self):
        f = Fixture("D1,К555ЛН1,,14,7,14,,,\nXP1,ОНП-ЖГ-79-9,,9,,,,,\n",
                    "A,D1,1,,1,\nA,XP1,3,,1,\nGND,XP1,1,,1,\nB,D1,2,,1,\n",
                    analog="XP1,port,,the printer port\n")
        text, problems = f.generate()
        self.assertEqual(errors(problems), [])
        self.assertIn("input  logic x_XP1_p3", text)
        self.assertNotIn("x_XP1_p1", text)

    def test_two_pulls_on_one_net_disagreeing_is_an_error(self):
        f = Fixture("D1,К555ЛН1,,14,7,14,,,\nR1,С1-4,,2,,,,,\nR2,С1-4,,2,,,,,\n",
                    "A,D1,1,,1,\nA,R1,1,,1,\n+5V,R1,2,,1,\nA,R2,1,,1,\nGND,R2,2,,1,\n"
                    "B,D1,2,,1,\n",
                    analog="R2,pull0,,a pull-down\n")
        _, problems = f.generate()
        self.assertTrue(any("pulled both" in e for e in errors(problems)))


CRYSTAL = """// A crystal oscillator.
module crystal #(
    parameter int PERIOD = 2, parameter int HIGH = 1
) (
    input  logic clk,
    output logic p1
);
    assign p1 = 1'b0;
endmodule
"""


class Crystals(unittest.TestCase):
    def test_periods_in_master_ticks_from_the_rating(self):
        self.assertEqual(board.crystal_ticks("8 МГц"), (15, 8))
        self.assertEqual(board.crystal_ticks("15 МГц"), (8, 4))

    def test_a_rating_off_the_master_clock_is_refused(self):
        with self.assertRaises(ValueError):
            board.crystal_ticks("7 МГц")

    def test_a_crystal_drives_its_first_pin_and_leaves_the_second(self):
        f = Fixture("BQ1,РК 169МА-6АН,15 МГц,2,,,,,\nD1,К555ЛН1,,14,7,14,,,\n",
                    "X,BQ1,1,,1,\nX,D1,1,,1,\nY,BQ1,2,,1,\nY,D1,2,,1,\n",
                    analog="BQ1,crystal,,the 15 MHz oscillator\n",
                    modules=(INVERTER, CRYSTAL))
        text, problems = f.generate()
        self.assertEqual(errors(problems), [])
        self.assertIn("crystal #(.PERIOD(8), .HIGH(4)) BQ1 (", text)
        self.assertIn("assign n_X = d_BQ1_p1;", text)
        self.assertIn("assign n_Y = d_D1_p2;", text)
        self.assertNotIn("x_BQ1", text)


DELAY_LINE = """// A delay line.
module delay_line #(
    parameter int TICKS = 1
) (
    input  logic clk,
    input  logic i,
    output logic o
);
    assign o = i;
endmodule
"""


class Delays(unittest.TestCase):
    def test_nanoseconds_become_master_ticks(self):
        self.assertEqual(board.delay_ticks(10.0, clocked=False), 1)
        self.assertEqual(board.delay_ticks(3.0, clocked=False), 0)
        self.assertEqual(board.delay_ticks(21.0, clocked=False), 3)
        # a clocked chip's own model already takes a tick
        self.assertEqual(board.delay_ticks(21.0, clocked=True), 2)
        self.assertEqual(board.delay_ticks(5.0, clocked=True), 0)

    def generate(self, delays, modules):
        f = Fixture("D1,К555ЛН1,,14,7,14,,,\nD2,К555АП5,,20,10,20,,,\n",
                    "A,D1,1,,1,\nB,D1,2,,1,\nE,D2,1,,1,\nB,D2,2,,1,\nY,D2,18,,1,\n",
                    modules=modules)
        f.write("delays.csv", "type,ns,source\n" + delays)
        with f.dir as d:
            return board.generate(board.Sources(
                parts=os.path.join(d, "parts.csv"), pinouts=os.path.join(d, "pinouts.csv"),
                nets=os.path.join(d, "nets.csv"), changes=None,
                analog=os.path.join(d, "analog.csv"), chips=os.path.join(d, "chips"),
                delays=os.path.join(d, "delays.csv")))

    def test_an_output_comes_through_a_delay_line(self):
        text, problems = self.generate("К555ЛН1,20,a datasheet\nК555АП5,3,a datasheet\n",
                                       (INVERTER, BUFFER, DELAY_LINE))
        self.assertEqual(errors(problems), [])
        self.assertIn(".p2(r_D1_p2)", text)
        self.assertIn("delay_line #(.TICKS(2)) l_D1_p2 (.clk(clk), .i(r_D1_p2), .o(d_D1_p2));", text)
        self.assertIn("input  logic clk", text)
        # under a tick: straight through, no line
        self.assertIn(".p18(d_D2_p18)", text)
        self.assertNotIn("l_D2_p18", text)

    def test_an_enable_has_a_delay_of_its_own(self):
        f = Fixture("D2,К555АП5,,20,10,20,,,\n", "E,D2,1,,1,\nB,D2,2,,1,\nY,D2,18,,1,\n",
                    modules=(BUFFER, DELAY_LINE))
        f.write("delays.csv", "type,ns,oe_ns,off_ns,source\nК555АП5,20,40,10,a datasheet\n")
        with f.dir as d:
            text, problems = board.generate(board.Sources(
                parts=os.path.join(d, "parts.csv"), pinouts=os.path.join(d, "pinouts.csv"),
                nets=os.path.join(d, "nets.csv"), changes=None,
                analog=os.path.join(d, "analog.csv"), chips=os.path.join(d, "chips"),
                delays=os.path.join(d, "delays.csv")))
        self.assertEqual(errors(problems), [])
        self.assertIn("delay_line #(.TICKS(2)) l_D2_p18 (.clk(clk), .i(r_D2_p18), .o(d_D2_p18));", text)
        self.assertIn("delay_line #(.TICKS(5), .FALL(1)) l_D2_p18_oe (.clk(clk), .i(r_D2_p18_oe), "
                      ".o(d_D2_p18_oe));", text)

    def test_a_chip_type_without_a_delay_is_an_error(self):
        _, problems = self.generate("К555ЛН1,10,a datasheet\n", (INVERTER, BUFFER, DELAY_LINE))
        self.assertTrue(any("К555АП5" in e and "delays.csv" in e for e in errors(problems)))


class TheBoard(unittest.TestCase):
    def test_ns4_generates_without_errors_and_matches_the_committed_file(self):
        text, problems = board.generate(board.ns4_sources())
        self.assertEqual(errors(problems), [])
        with open(os.path.join(ROOT, "rtl", "board", "ns4.sv"), encoding="utf-8") as f:
            self.assertEqual(f.read(), text, "rtl/board/ns4.sv is stale: run tools/board.py")


if __name__ == "__main__":
    unittest.main()
