// КР580ВВ51А (Intel 8251A) at its pins, by the Intel datasheet
// (docs/vv51.md): the asynchronous modes.  A mode word for the synchronous
// mode is taken and its sync characters awaited, as the reset sequence
// needs, but synchronous operation is not modelled - НС4 uses none.
//
// TxC and RxC are seen at the rising edges of CLK, as the chip samples
// them; a write is taken as -WR rises with the bus as it stood a master
// tick before; a data read clears RxRDY as -RD falls.
module kr580vv51 (
    input  logic clk,
    // D7..D0 (pins 8, 7, 6, 5, 2, 1, 28, 27)
    input  logic p8_in, p7_in, p6_in, p5_in, p2_in, p1_in, p28_in, p27_in,
    output logic p8, p7, p6, p5, p2, p1, p28, p27,
    output logic p8_oe, p7_oe, p6_oe, p5_oe, p2_oe, p1_oe, p28_oe, p27_oe,
    input  logic p20, p21,              // CLK RESET
    input  logic p12, p11, p13, p10,    // C/-D -CS -RD -WR
    input  logic p9, p25,               // -TxC -RxC
    input  logic p3,                    // RxD
    input  logic p17, p22,              // -CTS -DSR
    output logic p19,                   // TxD
    output logic p15, p18, p14,         // TxRDY TxEMPTY RxRDY
    output logic p16,                   // SYNDET/BD: break detect
    output logic p24, p23               // -DTR -RTS
);
    typedef enum logic [1:0] { S_MODE, S_SYNC1, S_SYNC2, S_CMD } seq_t;
    seq_t       seq;
    logic [7:0] d_in, d_was, mode, tx_buf;
    logic       cd_was, cs_was, wr_was, rd_was, wrote, read_start;
    logic       txen, dtr, rxe, sbrk, rts, internal_reset;
    logic       full, rxrdy, pe, oe, fe;
    logic       clk_was, txc_was, rxc_was, tx_fall, rx_rise;
    logic       take, busy, rx_done, rx_pe, rx_fe;
    logic [7:0] rx_data, rx_buf;

    assign d_in = {p8_in, p7_in, p6_in, p5_in, p2_in, p1_in, p28_in, p27_in};

    always_ff @(posedge clk) begin
        d_was <= d_in; cd_was <= p12; cs_was <= p11; wr_was <= p10; rd_was <= p13;
        clk_was <= p20;
        if (p20 && !clk_was) begin txc_was <= p9; rxc_was <= p25; end
    end
    assign tx_fall    = p20 && !clk_was && txc_was && !p9;
    assign rx_rise    = p20 && !clk_was && !rxc_was && p25;
    assign wrote      = p10 && !wr_was && !cs_was;
    assign read_start = !p13 && rd_was && !p11;

    always_ff @(posedge clk) begin
        internal_reset <= 1'b0;
        if (p21) begin
            seq <= S_MODE; mode <= '0;
            {txen, dtr, rxe, sbrk, rts} <= '0;
            {full, rxrdy, pe, oe, fe} <= '0;
        end else begin
            if (wrote && !cd_was) begin
                tx_buf <= d_was;
                full   <= 1'b1;
            end else if (take)
                full <= 1'b0;
            if (wrote && cd_was) control(d_was);
            if (read_start && !p12) rxrdy <= 1'b0;
            if (rx_done) begin
                rx_buf <= rx_data;
                if (rxrdy || (read_start && !p12)) oe <= 1'b1;
                rxrdy <= rxe;
                pe <= pe | rx_pe;
                fe <= fe | rx_fe;
            end
            if (!rxe) rxrdy <= 1'b0;
        end
    end

    task automatic control(input logic [7:0] w);
        case (seq)
            S_MODE: begin
                mode <= w;
                seq  <= w[1:0] != 2'b00 ? S_CMD : (w[7] ? S_SYNC2 : S_SYNC1);
            end
            S_SYNC1: seq <= S_SYNC2;
            S_SYNC2: seq <= S_CMD;
            default: begin
                {rts, sbrk, rxe, dtr, txen} <= {w[5], w[3], w[2], w[1], w[0]};
                if (w[4]) {pe, oe, fe} <= '0;
                if (w[6]) begin                         // internal reset
                    seq <= S_MODE;
                    {txen, dtr, rxe, sbrk, rts} <= '0;
                    full <= 1'b0;
                    internal_reset <= 1'b1;
                end
            end
        endcase
    endtask

    kr580vv51_tx tx (
        .clk, .reset(p21 || internal_reset), .fall(tx_fall),
        .factor(mode[1:0]), .length(mode[3:2]), .pen(mode[4]), .ep(mode[5]), .stops(mode[7:6]),
        .enable(txen && !p17), .sbrk, .full, .data(tx_buf), .take, .busy, .txd(p19)
    );

    kr580vv51_rx rx (
        .clk, .master_reset(p21), .rise(rx_rise), .rxd(p3),
        .factor(mode[1:0]), .length(mode[3:2]), .pen(mode[4]), .ep(mode[5]), .enable(rxe),
        .done(rx_done), .data(rx_data), .perr(rx_pe), .ferr(rx_fe), .brk(p16)
    );

    logic [7:0] status, bus;
    logic       reading;
    assign status  = {!p22, p16, fe, oe, pe, !full && !busy, rxrdy, !full};
    assign reading = !p11 && !p13;
    assign bus     = p12 ? status : rx_buf;
    // D drives a master tick after -RD and -CS fall and floats a tick after
    // they rise: inside tRD and tDF (docs/vv51.md), and a flip-flop away from
    // the bus for the FPGA
    logic       driving;
    logic [7:0] out_byte;
    always_ff @(posedge clk) begin
        driving  <= reading;
        out_byte <= reading ? bus : 8'h00;
    end
    assign {p8, p7, p6, p5, p2, p1, p28, p27} = out_byte;
    assign {p8_oe, p7_oe, p6_oe, p5_oe, p2_oe, p1_oe, p28_oe, p27_oe} = {8{driving}};

    assign p15 = !full && txen && !p17;     // the pin is gated, the status bit is not
    assign p18 = !full && !busy;
    assign p14 = rxrdy;
    assign p24 = !dtr;
    assign p23 = !rts;
endmodule
