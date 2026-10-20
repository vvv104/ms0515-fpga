// КР1807ВМ1 (DEC T-11): the processor at its pins.  The bus side
// (kr1807vm1_bus) puts one transaction at a time on the pins; the core
// (kr1807vm1_core) runs the instructions as those transactions.  The modes
// modelled and every choice made are in docs/t11-model.md: a 16-bit
// static bus, the clock on XTL1 with XTL0 grounded.
//
// A pin both read and driven (the DALs) is three ports: pN_in, the net's
// level, and pN, pN_oe, what the chip drives.
module kr1807vm1 (
    input  logic clk,
    // DAL15..DAL9 (pins 1-7), DAL8 (9), DAL7..DAL0 (10-17)
    input  logic p1_in,  p2_in,  p3_in,  p4_in,  p5_in,  p6_in,  p7_in,  p9_in,
    input  logic p10_in, p11_in, p12_in, p13_in, p14_in, p15_in, p16_in, p17_in,
    output logic p1,  p2,  p3,  p4,  p5,  p6,  p7,  p9,
    output logic p10, p11, p12, p13, p14, p15, p16, p17,
    output logic p1_oe,  p2_oe,  p3_oe,  p4_oe,  p5_oe,  p6_oe,  p7_oe,  p9_oe,
    output logic p10_oe, p11_oe, p12_oe, p13_oe, p14_oe, p15_oe, p16_oe, p17_oe,
    output logic p18,                   // -BCLR
    input  logic p19,                   // PUP
    output logic p21,                   // COUT
    input  logic p22,                   // XTL1
    input  logic p23,                   // XTL0
    output logic p24, p25,              // SEL1, SEL0
    input  logic p26,                   // READY
    output logic p27, p27_oe,           // R/-WHB
    output logic p28, p28_oe,           // R/-WLB
    output logic p29, p30, p31,         // -RAS, -CAS, PI
    input  logic p32, p33, p34, p35, p36, p37, p38, p39     // AI0..AI7
);

    logic [15:0] dal_in, dal_out, dal_oe;
    logic [7:0]  ai_in;
    logic        xtl, run, bclr, delayed, cout_pulse, rw_oe;
    logic        phase, take, data_valid, ai_valid, ai_requests, vector_valid;
    logic [15:0] data, req_addr, req_data;
    logic [7:0]  ai;
    logic [2:0]  req_kind;
    logic        req_fetch;
    logic [1:0]  req_bytes;
    logic [4:0]  req_ack;

    assign dal_in = {p1_in, p2_in, p3_in, p4_in, p5_in, p6_in, p7_in, p9_in,
                     p10_in, p11_in, p12_in, p13_in, p14_in, p15_in, p16_in, p17_in};
    assign {p1, p2, p3, p4, p5, p6, p7, p9, p10, p11, p12, p13, p14, p15, p16, p17} = dal_out;
    assign {p1_oe, p2_oe, p3_oe, p4_oe, p5_oe, p6_oe, p7_oe, p9_oe,
            p10_oe, p11_oe, p12_oe, p13_oe, p14_oe, p15_oe, p16_oe, p17_oe} = dal_oe;
    assign ai_in  = {p39, p38, p37, p36, p35, p34, p33, p32};
    // an external clock on XTL1 with XTL0 grounded - the oscillator's two
    // ends taken as their difference
    assign xtl    = p22 ^ p23;
    assign run    = !p19;
    assign p18    = !bclr;
    assign p27_oe = rw_oe;
    assign p28_oe = rw_oe;

    kr1807vm1_bus bus (
        .clk, .xtl, .run, .bclr,
        .req_kind, .req_fetch, .req_addr, .req_data, .req_bytes, .req_ack,
        .delayed, .cout_pulse,
        .phase, .take, .data_valid, .data, .ai_valid, .ai_requests, .ai, .vector_valid,
        .dal_in, .dal_out, .dal_oe, .ai_in, .ready_n(p26),
        .ras_n(p29), .cas_n(p30), .pi(p31), .cout(p21), .sel0(p25), .sel1(p24),
        .rwhb(p27), .rwlb(p28), .rw_oe
    );

    kr1807vm1_core core (
        .clk, .run,
        .phase, .take, .data_valid, .data, .ai_valid, .ai_requests, .ai, .vector_valid,
        .mode_in({dal_in[15:13], dal_in[8], dal_in[0]}),
        .req_kind, .req_fetch, .req_addr, .req_data, .req_bytes, .req_ack,
        .bclr, .delayed, .cout_pulse
    );
endmodule
