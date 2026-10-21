// КР580ВВ55А (Intel 8255A) at its pins, by the Intel datasheet
// (docs/vv55.md).  Mode 0 and the port C bit set/reset word; a mode word
// asking for mode 1 or 2 is taken as mode 0 with its directions - both
// PPIs on НС4 run in mode 0, the strobed modes are not modelled.
// A write is taken as -WR rises with the bus as it stood a master tick
// before; RESET (high) and every mode word clear the output latches;
// RESET makes every port an input.  A port set as output reads back its
// latch (the datasheet does not say; the emulator does so).
module kr580vv55 (
    input  logic clk,
    // D7..D0 (pins 27-34)
    input  logic p27_in, p28_in, p29_in, p30_in, p31_in, p32_in, p33_in, p34_in,
    output logic p27, p28, p29, p30, p31, p32, p33, p34,
    output logic p27_oe, p28_oe, p29_oe, p30_oe, p31_oe, p32_oe, p33_oe, p34_oe,
    // PA7..PA4 (37-40), PA3..PA0 (1-4)
    input  logic p37_in, p38_in, p39_in, p40_in, p1_in, p2_in, p3_in, p4_in,
    output logic p37, p38, p39, p40, p1, p2, p3, p4,
    output logic p37_oe, p38_oe, p39_oe, p40_oe, p1_oe, p2_oe, p3_oe, p4_oe,
    // PB7..PB0 (25-18)
    input  logic p25_in, p24_in, p23_in, p22_in, p21_in, p20_in, p19_in, p18_in,
    output logic p25, p24, p23, p22, p21, p20, p19, p18,
    output logic p25_oe, p24_oe, p23_oe, p22_oe, p21_oe, p20_oe, p19_oe, p18_oe,
    // PC7..PC4 (10-13), PC3..PC0 (17-14)
    input  logic p10_in, p11_in, p12_in, p13_in, p17_in, p16_in, p15_in, p14_in,
    output logic p10, p11, p12, p13, p17, p16, p15, p14,
    output logic p10_oe, p11_oe, p12_oe, p13_oe, p17_oe, p16_oe, p15_oe, p14_oe,
    input  logic p9, p8,                            // A0 A1
    input  logic p6, p5, p36, p35                   // -CS -RD -WR RESET
);
    logic [7:0] d_in, d_was, pa_in, pb_in, pc_in, a_q, b_q, c_q, bus;
    logic [1:0] a_was;
    logic       cs_was, wr_was, wrote, reading;
    logic       a_in, b_in, cu_in, cl_in;           // the directions: 1 = input

    assign d_in  = {p27_in, p28_in, p29_in, p30_in, p31_in, p32_in, p33_in, p34_in};
    assign pa_in = {p37_in, p38_in, p39_in, p40_in, p1_in, p2_in, p3_in, p4_in};
    assign pb_in = {p25_in, p24_in, p23_in, p22_in, p21_in, p20_in, p19_in, p18_in};
    assign pc_in = {p10_in, p11_in, p12_in, p13_in, p17_in, p16_in, p15_in, p14_in};

    always_ff @(posedge clk) begin
        d_was  <= d_in;
        a_was  <= {p8, p9};
        cs_was <= p6;
        wr_was <= p36;
        if (p35) begin
            {a_in, b_in, cu_in, cl_in} <= 4'b1111;
            a_q <= '0; b_q <= '0; c_q <= '0;
        end else if (wrote) begin
            case (a_was)
                2'd0: a_q <= d_was;
                2'd1: b_q <= d_was;
                2'd2: c_q <= d_was;
                default:
                    if (d_was[7]) begin                     // mode set
                        a_in <= d_was[4]; cu_in <= d_was[3];
                        b_in <= d_was[1]; cl_in <= d_was[0];
                        a_q <= '0; b_q <= '0; c_q <= '0;
                    end else                                 // PC bit set/reset
                        c_q[d_was[3:1]] <= d_was[0];
            endcase
        end
    end
    assign wrote = p36 && !wr_was && !cs_was;

    // a read: the pins of an input, the latch of an output
    always_comb begin
        case ({p8, p9})
            2'd0: bus = a_in ? pa_in : a_q;
            2'd1: bus = b_in ? pb_in : b_q;
            2'd2: bus = {cu_in ? pc_in[7:4] : c_q[7:4], cl_in ? pc_in[3:0] : c_q[3:0]};
            default: bus = 8'h00;                   // the control word cannot be read
        endcase
    end
    assign reading = !p6 && !p5;
    assign {p27, p28, p29, p30, p31, p32, p33, p34} = bus;
    assign {p27_oe, p28_oe, p29_oe, p30_oe, p31_oe, p32_oe, p33_oe, p34_oe} = {8{reading}};

    assign {p37, p38, p39, p40, p1, p2, p3, p4} = a_q;
    assign {p37_oe, p38_oe, p39_oe, p40_oe, p1_oe, p2_oe, p3_oe, p4_oe} = {8{!a_in}};
    assign {p25, p24, p23, p22, p21, p20, p19, p18} = b_q;
    assign {p25_oe, p24_oe, p23_oe, p22_oe, p21_oe, p20_oe, p19_oe, p18_oe} = {8{!b_in}};
    assign {p10, p11, p12, p13, p17, p16, p15, p14} = c_q;
    assign {p10_oe, p11_oe, p12_oe, p13_oe} = {4{!cu_in}};
    assign {p17_oe, p16_oe, p15_oe, p14_oe} = {4{!cl_in}};
endmodule
