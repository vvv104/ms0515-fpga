// ext_ram: the memory expansion module's RAM, its sixteen КР565РУ7 (MD19..
// MD34, 512 KB), kept in DDR3 through MiSTer's DDRAM port (plan 6a, step 4):
// 4 Mbit do not fit the FPGA's block memory beside the board's own.
//
// At the pins it is the sixteen chips as rtl/chips/kr565ru7.sv has them,
// tick for tick: the row the tick before -RAS fell, the column as a -CAS
// falls, an early write with -WE low by then, else a read whose cell shows
// on DO while -CAS stays low, and a late write as -WE falls under -CAS.
// The chips are two banks of eight on one -RAS and one -WE: CAS0 the even
// ones (MD19, 21, ..), CAS1 the odd; chip MD(19 + 2i + b) is DK0i's in bank
// b, its DO q[2i + b].  The chips' outputs go back into the board before
// their delays, so the board adds tCAC as for the chips themselves.
//
// DDR3 answers in some 100-200 ns, and not always as soon, so a read is
// not fetched as -CAS falls: the module's address (MA00..MA17 - the ВВ55's
// ports and the counter D14, standing before a cycle begins) names the
// cell, and the 64-bit word that holds it is fetched as soon as the address
// changes.  The multiplexers D15, D17, D18 put MA00..07 and MA16 on the row
// and MA08..15 and MA17 on the column.  A word holds four rows of a column
// in both banks: the cell (bank b, row r, column c) is at word {c, r[8:2]},
// byte {r[1:0], b} - the counter steps the row, so bytes read in order
// share a word.  Writes go out as they come, a byte each.
//
// faults counts what the chips would have done otherwise: a read whose word
// was not fetched yet (its byte then wrong), a write that found the one
// before it not yet taken.  The tests hold it at zero.
module ext_ram #(
    parameter logic [28:0] BASE = 29'h0600_0000     // 0x30000000, the cores' part of DDR3
) (
    input  logic        clk,
    input  logic        reset,
    input  logic        ras_n,
    input  logic        cas0_n,
    input  logic        cas1_n,
    input  logic        we_n,
    input  logic [8:0]  ra,
    input  logic [7:0]  di,
    input  logic [17:2] ma,                     // MA00, MA01 pick a byte of the word: not needed
    output logic [15:0] q,
    output logic [15:0] q_oe,
    output logic [31:0] faults,
    // DDR3 (MiSTer's DDRAM_*, on clk)
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
    logic [8:0]  a_was, row;
    logic [8:0]  column [2];
    logic        ras_was, we_was;
    logic [1:0]  cas_was, reading;
    logic [7:0]  out [2];
    logic [1:0]  cas_n;
    assign cas_n = {cas1_n, cas0_n};

    // the word fetched, the one asked for and the one on its way
    logic [63:0] word;
    logic [15:0] tag, asked;
    logic        stale;
    logic        valid;
    logic        inflight;
    logic [15:0] wanted;
    assign wanted = {ma[17], ma[15:8], ma[16], ma[7:2]};

    // a write on its way out
    logic        w_pending;
    logic [15:0] w_word;
    logic [63:0] w_data;
    logic [7:0]  w_be;

    function automatic logic [15:0] word_of(input logic [8:2] r, input logic [8:0] c);
        return {c, r};
    endfunction
    function automatic logic [2:0] lane_of(input logic [1:0] r, input logic b);
        return {r, b};
    endfunction

    // this tick's writes: each bank's enable, the cell's word and lane
    logic [1:0]  wr;
    logic [15:0] wr_word [2];
    logic [2:0]  wr_lane [2];
    logic [1:0]  rd;
    always_comb begin
        for (int b = 0; b < 2; b++) begin
            wr[b] = 1'b0;
            rd[b] = 1'b0;
            wr_word[b] = word_of(row[8:2], ra);
            wr_lane[b] = lane_of(row[1:0], b[0]);
            if (!ras_n && !cas_n[b] && cas_was[b]) begin
                wr[b] = !we_n;                              // early write
                rd[b] = we_n;
            end else if (!ras_n && !cas_n[b] && !we_n && we_was) begin
                wr[b] = 1'b1;                               // late write
                wr_word[b] = word_of(row[8:2], column[b]);
            end
        end
    end

    // the pins
    always_ff @(posedge clk) begin
        a_was   <= ra;
        ras_was <= ras_n;
        cas_was <= cas_n;
        we_was  <= we_n;
        if (!ras_n && ras_was)
            row <= a_was;
        for (int b = 0; b < 2; b++) begin
            if (cas_n[b])
                reading[b] <= 1'b0;
            if (!ras_n && !cas_n[b] && cas_was[b])
                column[b] <= ra;
            if (rd[b]) begin
                reading[b] <= 1'b1;
                out[b]     <= word[8 * lane_of(row[1:0], b[0]) +: 8];
            end
        end
    end
    always_comb
        for (int i = 0; i < 8; i++)
            for (int b = 0; b < 2; b++) begin
                q[2 * i + b]    = out[b][i];
                q_oe[2 * i + b] = reading[b];
            end

    // DDR3: the write out first, then the word the address names
    logic        missed, crowded, send, fetch;
    always_comb begin
        missed = 1'b0;
        for (int b = 0; b < 2; b++)
            if (rd[b] && !(valid && tag == word_of(row[8:2], ra)))
                missed = 1'b1;
        send    = !ddr_busy && w_pending;
        fetch   = !ddr_busy && !w_pending && !inflight && !(valid && tag == wanted);
        crowded = (|wr && w_pending && !send) || (wr[0] && wr[1] && wr_word[0] != wr_word[1]);
    end

    // from the FPGA's load: no request standing, no read on its way, nothing
    // fetched - the core's reset leaves a request and a read to finish
    initial begin
        ddr_rd    = 1'b0;
        ddr_we    = 1'b0;
        inflight  = 1'b0;
        valid     = 1'b0;
        w_pending = 1'b0;
        faults    = '0;
    end

    assign ddr_burstcnt = 8'd1;
    always_ff @(posedge clk) begin
        if (!ddr_busy) begin
            ddr_rd <= fetch;
            ddr_we <= send;
        end
        if (send) begin
            ddr_addr  <= BASE + 29'(w_word);
            ddr_din   <= w_data;
            ddr_be    <= w_be;
            w_pending <= 1'b0;
        end
        if (fetch) begin
            ddr_addr <= BASE + 29'(wanted);
            asked    <= wanted;
            inflight <= 1'b1;
            stale    <= 1'b0;
        end
        if (ddr_dout_ready && inflight) begin
            inflight <= 1'b0;
            if (!stale) begin
                word  <= ddr_dout;
                tag   <= asked;
                valid <= 1'b1;
            end
        end
        if (|wr) begin
            if (!w_pending || send) begin
                w_pending <= 1'b1;
                w_word    <= wr[0] ? wr_word[0] : wr_word[1];
                w_be      <= '0;
            end
            for (int b = 0; b < 2; b++)
                if (wr[b]) begin
                    w_data[8 * wr_lane[b] +: 8] <= di;
                    w_be[wr_lane[b]]             <= 1'b1;
                    if (valid && tag == wr_word[b])
                        word[8 * wr_lane[b] +: 8] <= di;
                    if (inflight && asked == wr_word[b] || fetch && wanted == wr_word[b])
                        stale <= 1'b1;      // the word on its way is older than this byte
                end
        end
        if (missed || crowded)
            faults <= faults + 1'b1;
        if (reset) begin
            // a request standing stays until DDR3 takes it, and a read on
            // its way is still answered: its word is let go as it comes,
            // not taken for the next read's
            valid     <= 1'b0;
            stale     <= 1'b1;
            w_pending <= 1'b0;
            faults    <= '0;
        end
    end
endmodule
