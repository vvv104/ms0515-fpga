// ext_ram_check: the test's top (sim/parts/test_ext_ram.cpp) - the
// module's sixteen КР565РУ7 (rtl/chips/kr565ru7.sv) and ext_ram, which
// stands for them in the core, on the same pins: the chips' DO and the
// bridge's side by side, the bridge's DDR3 port out to the test's model.
module ext_ram_check (
    input  logic        clk,
    input  logic        reset,
    input  logic        ras_n,
    input  logic        cas0_n,
    input  logic        cas1_n,
    input  logic        we_n,
    input  logic [8:0]  ra,
    input  logic [7:0]  di,
    input  logic [17:2] ma,
    output logic [15:0] chip_q,
    output logic [15:0] chip_q_oe,
    output logic [15:0] q,
    output logic [15:0] q_oe,
    output logic [31:0] faults,
    input  logic        ddr_busy,
    output logic [7:0]  ddr_burstcnt,
    output logic [28:0] ddr_addr,
    input  logic [63:0] ddr_dout,
    input  logic        ddr_dout_ready,
    output logic        ddr_rd,
    output logic [63:0] ddr_din,
    output logic [7:0]  ddr_be,
    output logic        ddr_we
);
    for (genvar i = 0; i < 8; i++) begin : g_bit
        for (genvar b = 0; b < 2; b++) begin : g_bank
            kr565ru7 chip (
                .clk,
                .p2(di[i]), .p3(we_n), .p4(ras_n), .p15(b == 0 ? cas0_n : cas1_n),
                .p5(ra[0]), .p7(ra[1]), .p6(ra[2]), .p12(ra[3]),
                .p11(ra[4]), .p10(ra[5]), .p13(ra[6]), .p9(ra[7]), .p1(ra[8]),
                .p14(chip_q[2 * i + b]), .p14_oe(chip_q_oe[2 * i + b])
            );
        end
    end

    ext_ram bridge (
        .clk, .reset, .ras_n, .cas0_n, .cas1_n, .we_n, .ra, .di, .ma,
        .q, .q_oe, .faults,
        .ddr_busy, .ddr_burstcnt, .ddr_addr, .ddr_dout, .ddr_dout_ready,
        .ddr_rd, .ddr_din, .ddr_be, .ddr_we
    );
endmodule
