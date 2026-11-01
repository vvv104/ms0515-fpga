// emu: the MS 0515 as a MiSTer core (plan stage 7d) - the machine
// (rtl/ms0515.sv) under Template_MiSTer's sys/ (the submodule in
// boards/de10nano/template, unchanged), which wraps every core's `emu`:
// the HDMI scaler, the OSD menu, the SD card's images through hps_io.
//
// The machine runs on its 120 MHz master clock, made from the board's
// 50 MHz by a PLL; hps_io and the video run on it too.  Drive 0 is the
// menu's image, its formatter chosen in the menu; reset is a power cycle
// for the board's logic (rtl/ms0515.sv).
//
// The keyboard is outside the core, on a serial line (TTL, idle high), as
// the emulator takes one on a COM port: portboard running the МС 7004's
// firmware on a PC, through a USB-UART cable.  The menu puts it on the
// user port - USER_IO[0] in, USER_IO[1] out (open drain, the IO board's
// pull-up) - or on the HPS's UART (Linux's /dev/ttyS1), or nowhere.
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
    assign AUDIO_S = 0;
    assign AUDIO_L = 0;
    assign AUDIO_R = 0;
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
        "-;",
        "O[4:3],Keyboard,None,User port,HPS UART;",
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
    wire         img_mounted;
    wire         img_readonly;
    wire [63:0]  img_size;
    wire [31:0]  sd_lba;
    wire         sd_rd, sd_wr, sd_ack;
    wire [13:0]  sd_buff_addr;          // hps_io's; a 512-byte block takes the low 9 bits
    wire [7:0]   sd_buff_dout, sd_buff_din;
    wire         sd_buff_wr;

    hps_io #(.CONF_STR(CONF_STR)) hps_io (
        .clk_sys, .HPS_BUS, .EXT_BUS(), .gamma_bus(),
        .buttons, .status, .status_menumask(16'd0),
        .img_mounted, .img_readonly, .img_size,
        .sd_lba('{sd_lba}), .sd_blk_cnt('{6'd0}), .sd_rd, .sd_wr, .sd_ack,
        .sd_buff_addr, .sd_buff_dout, .sd_buff_din('{sd_buff_din}), .sd_buff_wr
    );

    // ---- the master clock: 120 MHz from 50 (boards/de10nano/pll.sv) ---------
    pll pll (.refclk(CLK_50M), .rst(RESET), .outclk_0(clk_sys), .locked(locked));

    // ---- the machine -------------------------------------------------------
    wire       reset = RESET | status[0] | buttons[1] | !locked;
    wire       ce_pix, hs, vs, hblank, vblank;
    wire [7:0] red, green, blue;

    // the keyboard's line: in through two flip-flops (it is not of this clock)
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
        .kbd_on(on_user | on_uart), .kbd_rx(rx_sync[1]), .kbd_tx,
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
    assign CLK_VIDEO = clk_sys;
    assign CE_PIXEL = ce_pix;
    assign VGA_R = red;
    assign VGA_G = green;
    assign VGA_B = blue;
    assign VGA_HS = hs;
    assign VGA_VS = vs;
    assign VGA_DE = !(hblank | vblank);

    assign LED_USER = sd_rd | sd_wr;
    assign LED_DISK = {1'b0, sd_rd | sd_wr};
endmodule
