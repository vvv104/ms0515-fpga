// ms0515: the machine for the MiSTer core (plan stage 7d) - the НС4 board
// (rtl/board/ns4_machine.sv), drive 0 on XP4 (rtl/parts/mc5305.sv), the
// picture (rtl/parts/ns4_video.sv) and the supply, as the bench has them
// (sim/board/power.h, xp4.h): what the bench plays is logic here.
//
// The supply: the monitor VD11/R58 holds D95.13 low for the first
// millisecond, as the supply rises - after the FPGA is loaded and again
// after `reset`, a power cycle for the board's logic (its memory keeps).
//
// The keyboard's line on XS2 as a serial port's (TTL, idle high): a
// keyboard outside the machine - portboard running the МС 7004's firmware
// on a PC, through a USB-UART cable - as the emulator takes one on a COM
// port.  The board's USART frames the bytes at its own speed (about 4808
// baud), so the core only carries the levels.  At XS2 the line idles low,
// behind the board's inverting driver and receiver (КР1102АП15,
// КР559ИП11): kbd_rx drives ВХКЛ L (XS2.3) inverted, kbd_tx is ВЫХКЛ L
// (XS2.2) inverted.  Without kbd_on nothing is plugged in: the pin is let go.
module ms0515 (
    input  logic        clk,            // the 120 MHz master clock
    input  logic        reset,
    // the keyboard's serial line
    input  logic        kbd_on,
    input  logic        kbd_rx,
    output logic        kbd_tx,
    // drive 0's image (hps_io's block device) and its formatter: 0 ОМЕГА, 1 vvv, 2 FDZ
    input  logic [1:0]  layout,
    input  logic        img_mounted,
    input  logic        img_readonly,
    input  logic [63:0] img_size,
    output logic [31:0] sd_lba,
    output logic        sd_rd,
    output logic        sd_wr,
    input  logic        sd_ack,
    input  logic [8:0]  sd_buff_addr,
    input  logic [7:0]  sd_buff_dout,
    input  logic        sd_buff_wr,
    output logic [7:0]  sd_buff_din,
    // the picture (ns4_video)
    output logic        ce_pix,
    output logic [7:0]  red,
    output logic [7:0]  green,
    output logic [7:0]  blue,
    output logic        hs,
    output logic        vs,
    output logic        hblank,
    output logic        vblank
);
    localparam logic [16:0] RISE = 17'd120_000;    // 1 ms

    logic [16:0] risen;                 // master ticks since the supply came on
    always_ff @(posedge clk)
        if (reset)
            risen <= '0;
        else if (risen != RISE)
            risen <= risen + 1'b1;

    // the board's lines to the drive, the drive's open collectors back
    logic vn0, dv, npr, shag, zap, rzp, vbp;
    logic index, track0, protect, data, ready;
    // the picture's nets
    logic ts15, sc, kc, zc, bright, ssi, ksi, sgi;
    // the keyboard's line out
    logic vyhkl;
    assign kbd_tx = !vyhkl;

    ns4_machine board (
        .clk,
        .x_VD11_p1(1'b0), .x_VD11_p1_oe(risen != RISE),
        .x_XP4_p8(1'b0),  .x_XP4_p8_oe(!index),
        .x_XP4_p26(1'b0), .x_XP4_p26_oe(!track0),
        .x_XP4_p28(1'b0), .x_XP4_p28_oe(!protect),
        .x_XP4_p30(1'b0), .x_XP4_p30_oe(!data),
        .x_XP4_p34(1'b0), .x_XP4_p34_oe(!ready),
        .x_XS2_p3(!kbd_rx), .x_XS2_p3_oe(kbd_on),
        .n_DV_H(dv), .n_KC_H(kc), .n_KSI_L(ksi), .n_NPR_L(npr), .n_RZP_L(rzp),
        .n_SC_H(sc), .n_SGI_L(sgi), .n_SHAG_L(shag), .n_SSI_L(ssi), .n_TS15_H(ts15),
        .n_VBP_H(vbp), .n_VN0_H(vn0), .n_VYHKL_L(vyhkl), .n_ZAP_L(zap), .n_ZC_H(zc),
        .nx_D69_9(bright)
    );

    mc5305 drive (
        .clk, .layout,
        .select0(vn0), .motor(dv), .direction(npr), .step(shag),
        .write_data(zap), .write_gate(rzp), .side(vbp),
        .index, .track0, .protect, .data, .ready,
        .img_mounted, .img_readonly, .img_size,
        .sd_lba, .sd_rd, .sd_wr, .sd_ack, .sd_buff_addr, .sd_buff_dout, .sd_buff_wr, .sd_buff_din
    );

    ns4_video video (
        .clk, .ts15, .blue(sc), .red(kc), .green(zc), .bright,
        .ssi_n(ssi), .ksi_n(ksi), .sgi_n(sgi),
        .ce_pix, .r(red), .g(green), .b(blue), .hs, .vs, .hblank, .vblank
    );
endmodule
