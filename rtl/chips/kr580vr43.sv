// КР580ВР43 (8243): the MCS-48's input/output expander - four 4-bit ports
// P4..P7 worked through the processor's P20..P23 and PROG (Intel 8243
// datasheet).  As PROG (pin 7) falls with CS (pin 6) low, P20..P23 give the
// order: P21 P20 the port (0 P4, 1 P5, 2 P6, 3 P7), P23 P22 the operation
// (0 read, 1 write, 2 OR, 3 AND).  For a write, OR or AND the processor puts
// the data on P20..P23 while PROG is low, and as PROG rises the port takes
// it - written, ORed or ANDed into its latch - and drives its pins from
// then on.  For a read the port's outputs go off and its pins come out on
// P20..P23 while PROG is low.  After power-on every port is off.  PROG's
// edges are seen on the master tick.
module kr580vr43 (
    input  logic clk,
    input  logic p6,  input  logic p7,                             // CS PROG
    input  logic p11_in, input  logic p10_in, input  logic p9_in, input  logic p8_in,  // P20..P23
    output logic p11, output logic p10, output logic p9, output logic p8,
    output logic p11_oe, output logic p10_oe, output logic p9_oe, output logic p8_oe,
    // P40..P43, P50..P53, P60..P63, P70..P73: driven, and read
    input  logic p2_in,  input  logic p3_in,  input  logic p4_in,  input  logic p5_in,
    input  logic p1_in,  input  logic p23_in, input  logic p22_in, input  logic p21_in,
    input  logic p20_in, input  logic p19_in, input  logic p18_in, input  logic p17_in,
    input  logic p13_in, input  logic p14_in, input  logic p15_in, input  logic p16_in,
    output logic p2,  output logic p3,  output logic p4,  output logic p5,
    output logic p1,  output logic p23, output logic p22, output logic p21,
    output logic p20, output logic p19, output logic p18, output logic p17,
    output logic p13, output logic p14, output logic p15, output logic p16,
    output logic p2_oe,  output logic p3_oe,  output logic p4_oe,  output logic p5_oe,
    output logic p1_oe,  output logic p23_oe, output logic p22_oe, output logic p21_oe,
    output logic p20_oe, output logic p19_oe, output logic p18_oe, output logic p17_oe,
    output logic p13_oe, output logic p14_oe, output logic p15_oe, output logic p16_oe
);
    logic [3:0]  p2x;
    logic [15:0] pins;                  // P7 P6 P5 P4, each bit 3 first
    logic [15:0] latch;
    logic [3:0]  on;                    // the ports driving their pins
    logic [1:0]  port, op;
    logic        prog_was, reading;
    logic        ordered;               // an order taken as PROG fell: a rise acts on it

    assign p2x = {p8_in, p9_in, p10_in, p11_in};
    assign pins = {p16_in, p15_in, p14_in, p13_in, p17_in, p18_in, p19_in, p20_in,
                   p21_in, p22_in, p23_in, p1_in, p5_in, p4_in, p3_in, p2_in};

    always_ff @(posedge clk) begin
        prog_was <= p7;
        if (prog_was && !p7 && !p6) begin               // PROG falls: the order
            port <= p2x[1:0];
            op <= p2x[3:2];
            ordered <= 1'b1;
            reading <= p2x[3:2] == 2'd0;
            if (p2x[3:2] == 2'd0)
                on[p2x[1:0]] <= 1'b0;
        end else if (!prog_was && p7 && ordered) begin   // PROG rises: the data
            ordered <= 1'b0;
            if (!reading && !p6) begin
                case (op)
                    2'd1:    latch[4 * port +: 4] <= p2x;
                    2'd2:    latch[4 * port +: 4] <= latch[4 * port +: 4] | p2x;
                    default: latch[4 * port +: 4] <= latch[4 * port +: 4] & p2x;
                endcase
                on[port] <= 1'b1;
            end
            reading <= 1'b0;
        end
    end

    // a read: the port's pins out on P20..P23 while PROG is low
    logic [3:0] read_q;
    assign read_q = pins[4 * port +: 4];
    assign {p8, p9, p10, p11} = read_q;
    assign {p8_oe, p9_oe, p10_oe, p11_oe} = {4{reading && !p7}};

    assign {p16, p15, p14, p13, p17, p18, p19, p20,
            p21, p22, p23, p1, p5, p4, p3, p2} = latch;
    assign {p16_oe, p15_oe, p14_oe, p13_oe} = {4{on[3]}};
    assign {p17_oe, p18_oe, p19_oe, p20_oe} = {4{on[2]}};
    assign {p21_oe, p22_oe, p23_oe, p1_oe} = {4{on[1]}};
    assign {p5_oe, p4_oe, p3_oe, p2_oe} = {4{on[0]}};
endmodule
