// КР580ВИ53 (Intel 8253): three 16-bit counters at the pins, by the Intel
// datasheet (docs/vi53.md).  A write is taken as -WR rises, with the bus
// as it stood the master tick before; a read drives D while -CS and -RD
// are low, and the counter's byte pointer moves on when -RD rises.  The
// control word at A1A0 = 11 is write-only; SC = 11 (the 8254's read-back)
// is ignored, the 8253 having none.
module kr580vi53 (
    input  logic clk,
    // D7..D0 (pins 1-8)
    input  logic p1_in, p2_in, p3_in, p4_in, p5_in, p6_in, p7_in, p8_in,
    output logic p1, p2, p3, p4, p5, p6, p7, p8,
    output logic p1_oe, p2_oe, p3_oe, p4_oe, p5_oe, p6_oe, p7_oe, p8_oe,
    input  logic p9,  p11, output logic p10,        // CLK0 GATE0 OUT0
    input  logic p15, p14, output logic p13,        // CLK1 GATE1 OUT1
    input  logic p18, p16, output logic p17,        // CLK2 GATE2 OUT2
    input  logic p19, p20,                          // A0 A1
    input  logic p21, p22, p23                      // -CS -RD -WR
);
    logic [7:0] d_in, d_was, rd_byte [3];
    logic [1:0] a_was;
    logic       cs_was, wr_was, rd_was, wrote, read_end;
    logic [2:0] cw_we, latch_we, data_we, read_done;
    logic       clk_pin [3], gate_pin [3], out_pin [3];

    assign d_in = {p1_in, p2_in, p3_in, p4_in, p5_in, p6_in, p7_in, p8_in};
    assign {clk_pin[0], clk_pin[1], clk_pin[2]}   = {p9, p15, p18};
    assign {gate_pin[0], gate_pin[1], gate_pin[2]} = {p11, p14, p16};
    assign {p10, p13, p17} = {out_pin[0], out_pin[1], out_pin[2]};

    // the bus a tick ago, and the strobes' ends
    always_ff @(posedge clk) begin
        d_was  <= d_in;
        a_was  <= {p20, p19};
        cs_was <= p21;
        wr_was <= p23;
        rd_was <= p22;
    end
    assign wrote    = p23 && !wr_was && !cs_was;
    assign read_end = p22 && !rd_was && !cs_was;

    always_comb begin
        cw_we = '0; latch_we = '0; data_we = '0; read_done = '0;
        if (wrote && a_was == 2'b11 && d_was[7:6] != 2'b11) begin
            if (d_was[5:4] == 2'b00) latch_we[d_was[7:6]] = 1'b1;
            else                     cw_we[d_was[7:6]] = 1'b1;
        end
        if (wrote && a_was != 2'b11) data_we[a_was] = 1'b1;
        if (read_end && a_was != 2'b11) read_done[a_was] = 1'b1;
    end

    genvar i;
    generate for (i = 0; i < 3; i = i + 1) begin : counter
        kr580vi53_counter c (
            .clk, .clk_pin(clk_pin[i]), .gate_pin(gate_pin[i]),
            .cw_we(cw_we[i]), .cw(d_was[5:0]), .latch_we(latch_we[i]),
            .data_we(data_we[i]), .data(d_was), .read_done(read_done[i]),
            .out(out_pin[i]), .read_byte(rd_byte[i])
        );
    end endgenerate

    logic reading;
    assign reading = !p21 && !p22 && {p20, p19} != 2'b11;
    assign {p1, p2, p3, p4, p5, p6, p7, p8} = reading ? rd_byte[{p20, p19}] : 8'h00;
    assign {p1_oe, p2_oe, p3_oe, p4_oe, p5_oe, p6_oe, p7_oe, p8_oe} = {8{reading}};
endmodule
