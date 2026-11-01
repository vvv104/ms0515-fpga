// emu: the MS 0515 as a MiSTer core (plan stage 7d) - the machine
// (rtl/ms0515.sv) under Template_MiSTer's sys/ (the submodule in
// boards/de10nano/template, unchanged), which wraps every core's `emu`:
// the HDMI scaler, the OSD menu, the SD card's images through hps_io.
//
// The machine runs on its 120 MHz master clock, made from the board's
// 50 MHz by a PLL, and hps_io with it; the pixels go to sys/ on the PLL's
// 60 MHz (rtl/parts/video_half.sv).  Drive 0 is the
// menu's image, its formatter chosen in the menu; reset is a power cycle
// for the board's logic (rtl/ms0515.sv).
//
// The keyboard, as the menu says: the МС 7004 inside the core, MiSTer's
// USB keyboard pressing its keys (rtl/ms0515.sv; by character or by
// position, the menu's Keys) and its bell on the audio out; or one outside
// on a serial line (TTL, idle high), as the emulator takes one on a COM
// port - portboard running the МС 7004's firmware on a PC through a
// USB-UART cable, or the МС 7004 itself through a level shifter - on the
// user port (USER_IO[0] in, USER_IO[1] out, open drain, the IO board's
// pull-up) or on the HPS's UART (Linux's /dev/ttyS1); or none.
//
// The menu's Load ROM puts another ROM in the board's sockets XS3/XS4:
// a 16 KB image (160000..177777 as little-endian words), ROM-A for OSA,
// written into the cells of D65/D77 while the machine is held in reset.
// It stays there until the core is loaded again.
//
// The board is the owner's НС4 rev. 3А (netlist/rev3a.csv); its one change
// from Э3, D70.15 lifted onto GND, can be taken back in the menu.
module emu
(
    `include "sys/emu_ports.vh"
);
    // ---- what this core leaves alone ------------------------------------
    assign ADC_BUS = 'Z;
    assign {SD_SCK, SD_MOSI, SD_CS} = 'Z;
    assign {SDRAM_DQ, SDRAM_A, SDRAM_BA, SDRAM_CLK, SDRAM_CKE, SDRAM_DQML, SDRAM_DQMH,
            SDRAM_nWE, SDRAM_nCAS, SDRAM_nRAS, SDRAM_nCS} = 'Z;
    assign {DDRAM_CLK, DDRAM_BURSTCNT, DDRAM_ADDR, DDRAM_DIN, DDRAM_BE, DDRAM_RD, DDRAM_WE} = '0;
    assign VGA_SL = 0;
    assign VGA_F1 = 0;
    assign VGA_SCALER = 0;
    assign VGA_DISABLE = 0;
    assign HDMI_FREEZE = 0;
    assign HDMI_BLACKOUT = 0;
    assign HDMI_BOB_DEINT = 0;
    assign AUDIO_MIX = 0;
    assign LED_POWER = 0;
    assign BUTTONS = 0;

    // ---- the menu ----------------------------------------------------------
    `include "build_id.v"
    localparam CONF_STR = {
        "MS0515;;",
        "-;",
        "S0,DSKIMG,Drive 0;",
        "O[2:1],Disk layout,Omega,vvv,FDZ;",
        "F1,ROM,Load ROM;",
        "-;",
        "O[4:3],Keyboard,MS 7004 (USB),User port,HPS UART,None;",
        "O[5],Keys,By character,By position;",
        "-;",
        "O[6],D70.15 (rev. 3A),On GND,As drawn (E3);",
        "-;",
        "O[122:121],Aspect ratio,Original,Full Screen,[ARC1],[ARC2];",
        "-;",
        "T[0],Reset;",
        "R[0],Reset and close OSD;",
        "V,v", `BUILD_DATE
    };

    wire         clk_sys;
    wire         locked;
    wire [1:0]   buttons;
    wire [127:0] status;
    wire [10:0]  ps2_key;
    wire         kbd_bell;
    wire         img_mounted;
    wire         img_readonly;
    wire [63:0]  img_size;
    wire [31:0]  sd_lba;
    wire         sd_rd, sd_wr, sd_ack;
    wire [13:0]  sd_buff_addr;          // hps_io's; a 512-byte block takes the low 9 bits
    wire [7:0]   sd_buff_dout, sd_buff_din;
    wire         sd_buff_wr;
    wire         ioctl_download, ioctl_wr;
    wire [15:0]  ioctl_index;
    wire [26:0]  ioctl_addr;
    wire [7:0]   ioctl_dout;

    hps_io #(.CONF_STR(CONF_STR)) hps_io (
        .clk_sys, .HPS_BUS, .EXT_BUS(), .gamma_bus(),
        .buttons, .status, .status_menumask(16'd0), .ps2_key,
        .img_mounted, .img_readonly, .img_size,
        .sd_lba('{sd_lba}), .sd_blk_cnt('{6'd0}), .sd_rd, .sd_wr, .sd_ack,
        .sd_buff_addr, .sd_buff_dout, .sd_buff_din('{sd_buff_din}), .sd_buff_wr,
        .ioctl_download, .ioctl_index, .ioctl_wr, .ioctl_addr, .ioctl_dout,
        .ioctl_wait(1'b0)
    );

    // ---- the master clock: 120 MHz from 50 (boards/de10nano/pll.sv) ---------
    wire clk_video;                     // 60 MHz, in phase: sys/'s video side
    pll pll (.refclk(CLK_50M), .rst(RESET), .outclk_0(clk_sys), .outclk_1(clk_video), .locked(locked));

    // ---- the machine -------------------------------------------------------
    // a ROM image coming from the menu: the machine held in reset meanwhile
    wire       rom_loading = ioctl_download && ioctl_index[5:0] == 6'd1;
    wire       rom_we = rom_loading && ioctl_wr && ioctl_addr[26:14] == 13'd0;
    wire       reset = RESET | status[0] | buttons[1] | !locked | rom_loading;
    wire       ce_pix, hs, vs, hblank, vblank;
    wire [7:0] red, green, blue;

    // the keyboard's line: in through two flip-flops (it is not of this clock)
    wire       kbd_inside = status[4:3] == 2'd0;    // the МС 7004 inside, MiSTer's USB keyboard on it
    wire       on_user = status[4:3] == 2'd1;
    wire       on_uart = status[4:3] == 2'd2;
    reg  [1:0] rx_sync = 2'b11;
    wire       kbd_tx;
    always @(posedge clk_sys)
        rx_sync <= {rx_sync[0], on_user ? USER_IN[0] : on_uart ? UART_RXD : 1'b1};
    assign USER_OUT = {5'b11111, on_user ? kbd_tx : 1'b1, 1'b1};
    assign UART_TXD = on_uart ? kbd_tx : 1'b1;
    assign {UART_RTS, UART_DTR} = 0;

    ms0515 machine (
        .clk(clk_sys), .reset,
        .rom_we, .rom_addr(ioctl_addr[13:0]), .rom_data(ioctl_dout),
        .d70_drawn(status[6]),
        .kbd_source(kbd_inside ? 2'd1 : on_user || on_uart ? 2'd2 : 2'd0),
        .ps2_key, .kbd_mode(status[5]), .kbd_rx(rx_sync[1]), .kbd_tx,
        .kbd_leds(), .kbd_bell,
        .layout(status[2:1]),
        .img_mounted, .img_readonly, .img_size,
        .sd_lba, .sd_rd, .sd_wr, .sd_ack, .sd_buff_addr(sd_buff_addr[8:0]), .sd_buff_dout, .sd_buff_wr,
        .sd_buff_din,
        .ce_pix, .red, .green, .blue, .hs, .vs, .hblank, .vblank
    );

    // ---- the picture: 15 kHz lines, the scaler makes HDMI of them -----------
    wire [1:0] ar = status[122:121];
    assign VIDEO_ARX = (!ar) ? 13'd4 : 13'(ar - 1'd1);
    assign VIDEO_ARY = (!ar) ? 13'd3 : 13'd0;
    // the pixels onto the 60 MHz video clock (sys/'s scaler does not run at 120)
    wire       ce_video, hs_video, vs_video, hblank_video, vblank_video;
    video_half video_half (
        .clk(clk_sys), .ce_pix, .r(red), .g(green), .b(blue), .hs, .vs, .hblank, .vblank,
        .clk_video, .ce_out(ce_video), .r_out(VGA_R), .g_out(VGA_G), .b_out(VGA_B),
        .hs_out(hs_video), .vs_out(vs_video), .hblank_out(hblank_video), .vblank_out(vblank_video)
    );
    assign CLK_VIDEO = clk_video;
    assign CE_PIXEL = ce_video;
    assign VGA_HS = hs_video;
    assign VGA_VS = vs_video;
    assign VGA_DE = !(hblank_video | vblank_video);

    // the МС 7004's bell (its D2.3 and the piezo BA1): a square wave
    assign AUDIO_S = 1'b1;
    assign AUDIO_L = kbd_bell ? 16'sh1000 : -16'sh1000;
    assign AUDIO_R = AUDIO_L;

    assign LED_USER = sd_rd | sd_wr;
    assign LED_DISK = {1'b0, sd_rd | sd_wr};
endmodule
