// ms0515: the machine for the MiSTer core (plan stage 7d) - the НС4 board
// (rtl/board/ns4_machine.sv), drive 0 on XP4 (rtl/parts/mc5305.sv), the
// picture (rtl/parts/ns4_video.sv) and the supply, as the bench has them
// (sim/board/power.h, xp4.h): what the bench plays is logic here.
//
// The supply: the monitor VD11/R58 holds D95.13 low for the first
// millisecond, as the supply rises - after the FPGA is loaded and again
// after `reset`, a power cycle for the board's logic (its memory keeps).
//
// The keyboard on XS2 (kbd_source):
// - 1, the МС 7004 inside: its processor board from its netlist
//   (rtl/board/ms7004_machine.sv) running its own firmware, its matrix
//   pressed by a PC keyboard (MiSTer's ps2_key, rtl/parts/ms7004_keys.sv,
//   by character or by position: kbd_mode), its cable to XS2 - its Выход on
//   ВХКЛ L, ВЫХКЛ L on its Вход, the line's sense the same at both ends.
//   It comes up with the supply, as on one cable; its indicators and bell
//   come out (kbd_leds, kbd_bell);
// - 2, a keyboard outside on a serial line (TTL, idle high): portboard
//   running the МС 7004's firmware on a PC through a USB-UART cable, as the
//   emulator takes one on a COM port.  The board's USART frames the bytes
//   at its own speed (about 4808 baud), so the core only carries the
//   levels.  At XS2 the line idles low, behind the board's inverting driver
//   and receiver (КР1102АП15, КР559ИП11): kbd_rx drives ВХКЛ L (XS2.3)
//   inverted, kbd_tx is ВЫХКЛ L (XS2.2) inverted;
// - 0, nothing plugged in: the pin let go.
module ms0515 (
    input  logic        clk,            // the 120 MHz master clock
    input  logic        reset,
    // another ROM in the sockets XS3/XS4 (the menu loads ROM-A, which OSA
    // wants): its 16 KB, 140000..177777 as little-endian words (the window at
    // 160000 shows the upper 8 KB), a byte a tick
    input  logic        rom_we,
    input  logic [13:0] rom_addr,
    input  logic [7:0]  rom_data,
    // the owner's board's changes (netlist/vvv104.csv) taken back to Э3:
    // D70.15 on ~D14.3 as drawn, not on GND
    input  logic        d70_drawn,
    // the keyboard
    input  logic [1:0]  kbd_source,
    input  logic [10:0] ps2_key,
    input  logic        kbd_mode,       // 0 by character, 1 by position
    input  logic        kbd_rx,
    output logic        kbd_tx,
    output logic [4:0]  kbd_leds,       // lit: ОЖИД, КМП, ФКС, СТОП КАДР, ЛАТ
    output logic        kbd_bell,
    // the speaker's node (D93's open collectors, through R61 to XP7.2):
    // high at rest, low while current would flow through the coil
    output logic        speaker,
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
    // the keyboard's lines at XS2
    logic vyhkl, vyhod;
    assign kbd_tx = !vyhkl;

    ns4_machine board (
        .clk,
        .x_VD11_p1(1'b0), .x_VD11_p1_oe(risen != RISE),
        .x_XP4_p8(1'b0),  .x_XP4_p8_oe(!index),
        .x_XP4_p26(1'b0), .x_XP4_p26_oe(!track0),
        .x_XP4_p28(1'b0), .x_XP4_p28_oe(!protect),
        .x_XP4_p30(1'b0), .x_XP4_p30_oe(!data),
        .x_XP4_p34(1'b0), .x_XP4_p34_oe(!ready),
        .x_XS2_p3(kbd_source == 2'd1 ? vyhod : !kbd_rx), .x_XS2_p3_oe(kbd_source != 2'd0),
        .n_DV_H(dv), .n_KC_H(kc), .n_KSI_L(ksi), .n_NPR_L(npr), .n_RZP_L(rzp),
        .n_SC_H(sc), .n_SGI_L(sgi), .n_SHAG_L(shag), .n_SSI_L(ssi), .n_TS15_H(ts15),
        .n_VBP_H(vbp), .n_VN0_H(vn0), .n_VYHKL_L(vyhkl), .n_ZAP_L(zap), .n_ZC_H(zc),
        .nx_D69_9(bright), .nx_D93_3(speaker),
        .l_D65_we(rom_we && !rom_addr[0]), .l_D65_a(rom_addr[13:1]), .l_D65_d(rom_data),
        .l_D77_we(rom_we && rom_addr[0]),  .l_D77_a(rom_addr[13:1]), .l_D77_d(rom_data),
        .s_D70_15(d70_drawn)
    );

    mc5305 drive (
        .clk, .layout,
        .select0(vn0), .motor(dv), .direction(npr), .step(shag),
        .write_data(zap), .write_gate(rzp), .side(vbp),
        .index, .track0, .protect, .data, .ready,
        .img_mounted, .img_readonly, .img_size,
        .sd_lba, .sd_rd, .sd_wr, .sd_ack, .sd_buff_addr, .sd_buff_dout, .sd_buff_wr, .sd_buff_din
    );

    // ---- the МС 7004 inside --------------------------------------------------
    logic [15:0]  vr;
    logic [7:0]   gr;
    logic [127:0] keys;
    logic         lat_n, ozhid_n, kmp_n, fks_n, stop_n;

    ms7004_machine keyboard (
        .clk,
        .x_C3_p1(1'b0), .x_C3_p1_oe(risen != RISE),
        .x_XT1_p3(vyhkl), .x_XT1_p3_oe(1'b1),
        .x_XS_p24(gr[0]), .x_XS_p25(gr[1]), .x_XS_p26(gr[2]), .x_XS_p27(gr[3]),
        .x_XS_p28(gr[4]), .x_XS_p29(gr[5]), .x_XS_p31(gr[6]), .x_XS_p30(gr[7]),
        .x_XS_p24_oe(1'b1), .x_XS_p25_oe(1'b1), .x_XS_p26_oe(1'b1), .x_XS_p27_oe(1'b1),
        .x_XS_p28_oe(1'b1), .x_XS_p29_oe(1'b1), .x_XS_p30_oe(1'b1), .x_XS_p31_oe(1'b1),
        .n_VYHOD(vyhod),
        .n_VR1(vr[0]),   .n_VR2(vr[1]),   .n_VR3(vr[2]),   .n_VR4(vr[3]),
        .n_VR5(vr[4]),   .n_VR6(vr[5]),   .n_VR7(vr[6]),   .n_VR8(vr[7]),
        .n_VR9(vr[8]),   .n_VR10(vr[9]),  .n_VR11(vr[10]), .n_VR12(vr[11]),
        .n_VR13(vr[12]), .n_VR14(vr[13]), .n_VR15(vr[14]), .n_VR16(vr[15]),
        .n_OZHID(ozhid_n), .n_KMP(kmp_n), .n_FKS(fks_n), .n_STOP_KADR(stop_n), .n_LAT(lat_n),
        .nx_D2_6(kbd_bell)
    );
    assign kbd_leds = ~{ozhid_n, kmp_n, fks_n, stop_n, lat_n};

    ms7004_matrix matrix (.pressed(keys), .vr, .gr);

    ms7004_keys translator (
        .clk, .ps2_key, .mode(kbd_mode), .rus(lat_n), .pressed(keys)
    );

    ns4_video video (
        .clk, .ts15, .blue(sc), .red(kc), .green(zc), .bright,
        .ssi_n(ssi), .ksi_n(ksi), .sgi_n(sgi),
        .ce_pix, .r(red), .g(green), .b(blue), .hs, .vs, .hblank, .vblank
    );
endmodule
