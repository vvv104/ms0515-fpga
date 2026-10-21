// One counter of КР580ВИ53 (see kr580vi53.sv), by the Intel 8253
// datasheet (docs/vi53.md).  CLK and GATE are pins sampled on the master
// clock: GATE is taken at CLK rising, the counter acts at CLK falling.  A
// count written reaches the counting element at the first CLK fall after a
// CLK rise that follows the write; that fall does not count.
module kr580vi53_counter (
    input  logic       clk,
    input  logic       clk_pin, gate_pin,
    input  logic       cw_we,           // a control word for this counter (not a latch)
    input  logic [5:0] cw,              // RL1 RL0 M2 M1 M0 BCD
    input  logic       latch_we,        // the counter latch command
    input  logic       data_we,         // a count byte written
    input  logic [7:0] data,
    input  logic       read_done,       // a read byte taken (-RD rose)
    output logic       out,
    output logic [7:0] read_byte
);
    logic [1:0]  rl;
    logic [2:0]  mode;
    logic        bcd;
    logic [15:0] cr, ce, ol;            // count register, counting element, output latch
    logic        latched, wr_msb, rd_msb;
    logic        loading;               // a count to move into the element
    logic        armed;                 // ... CLK has risen since: the next fall loads
    logic        counting, out_q, strobe;
    logic        clk_was, gate_s, trig;

    // the 8253's modes 2 and 3 take M2 as don't care
    function automatic logic [2:0] mode_of(input logic [2:0] m);
        return m[1] ? {1'b0, m[1:0]} : m;
    endfunction

    // n subtracted from a count, in binary or in four BCD decades
    function automatic logic [15:0] minus(input logic [15:0] v, input logic [1:0] n, input logic b);
        logic [15:0] r;
        logic [3:0]  d;
        logic        borrow;
        if (!b) return v - {14'd0, n};
        r = v;
        for (int k = 0; k < n; k++) begin
            borrow = 1'b1;
            for (int i = 0; i < 4; i++) begin
                d = r[4 * i +: 4];
                if (borrow) begin
                    borrow   = d == 0;
                    r[4 * i +: 4] = d == 0 ? 4'd9 : d - 4'd1;
                end
            end
        end
        return r;
    endfunction

    // modes 2 and 3 hold OUT high while GATE is low, at once
    assign out = (mode == 3'd2 || mode == 3'd3) && !gate_pin ? 1'b1 : out_q;
    assign read_byte = rl == 2'b10 || (rl == 2'b11 && rd_msb) ? (latched ? ol[15:8] : ce[15:8])
                                                             : (latched ? ol[7:0] : ce[7:0]);

    always_ff @(posedge clk) begin
        clk_was <= clk_pin;
        if (cw_we) control();
        if (latch_we && !latched) begin
            ol      <= ce;
            latched <= 1'b1;
        end
        if (data_we) count_byte();
        if (read_done) begin
            if (rl != 2'b11 || rd_msb) latched <= 1'b0;
            if (rl == 2'b11) rd_msb <= !rd_msb;
        end
        if (clk_pin && !clk_was) begin                  // CLK rising: GATE sampled
            gate_s <= gate_pin;
            trig   <= trig || (gate_pin && !gate_s);
            if (loading) armed <= 1'b1;
        end
        if (!clk_pin && clk_was && !cw_we && !data_we)  // CLK falling: the counter acts
            fall();
    end

    task automatic control();
        rl <= cw[5:4]; mode <= mode_of(cw[3:1]); bcd <= cw[0];
        wr_msb <= 1'b0; rd_msb <= 1'b0; latched <= 1'b0;
        loading <= 1'b0; armed <= 1'b0; counting <= 1'b0; strobe <= 1'b0; trig <= 1'b0;
        out_q <= mode_of(cw[3:1]) != 3'd0;
    endtask

    task automatic count_byte();
        logic [15:0] v;
        logic        done;
        v    = cr;
        done = 1'b1;
        case (rl)
            2'b01: v = {8'd0, data};
            2'b10: v = {data, 8'd0};
            default: begin
                if (!wr_msb) begin
                    v[7:0] = data;
                    done   = 1'b0;
                end else
                    v[15:8] = data;
                wr_msb <= !wr_msb;
            end
        endcase
        cr <= v;
        if (mode == 3'd0) begin
            out_q <= 1'b0;
            counting <= done;                   // the first byte stops it, the last starts it
        end
        // modes 0 and 4 load every count; 2 and 3 only the first, then at the reload
        if (done && (mode == 3'd0 || mode == 3'd4 ||
                     ((mode == 3'd2 || mode == 3'd3) && !counting))) begin
            loading <= 1'b1;
            armed   <= 1'b0;
        end
    endtask

    task automatic fall();
        trig <= 1'b0;
        if (strobe) begin                       // modes 4 and 5: one clock low
            out_q  <= 1'b1;
            strobe <= 1'b0;
        end
        if (armed) begin                        // the load clock
            ce <= cr; loading <= 1'b0; armed <= 1'b0; counting <= 1'b1;
            if (mode == 3'd2 || mode == 3'd3) out_q <= 1'b1;
        end else if ((mode == 3'd1 || mode == 3'd5) && trig) begin
            ce <= cr; counting <= 1'b1;
            if (mode == 3'd1) out_q <= 1'b0;
        end else if ((mode == 3'd2 || mode == 3'd3) && trig && counting) begin
            ce <= cr; out_q <= 1'b1;            // a GATE rise restarts the period
        end else if (counting && !loading)
            step();      // (not before a new count is in)
    endtask

    // one CLK fall of counting
    task automatic step();
        logic [15:0] n;
        logic        gated;
        gated = (mode == 3'd1 || mode == 3'd5) || gate_s;
        if (!gated) return;
        case (mode)
            3'd2: begin
                if (ce == 16'd2)      begin ce <= 16'd1; out_q <= 1'b0; end
                else if (ce == 16'd1) begin ce <= cr;    out_q <= 1'b1; end
                else ce <= minus(ce, 2'd1, bcd);
            end
            3'd3: begin
                n = minus(ce, ce[0] ? (out_q ? 2'd1 : 2'd3) : 2'd2, bcd);
                if (n == 16'd0) begin ce <= cr; out_q <= !out_q; end
                else ce <= n;
            end
            default: begin                      // 0, 1, 4, 5
                n = minus(ce, 2'd1, bcd);
                ce <= n;
                if (n == 16'd0) begin
                    if (mode == 3'd0 || mode == 3'd1) out_q <= 1'b1;
                    else begin out_q <= 1'b0; strobe <= 1'b1; end
                    if (mode != 3'd0) counting <= 1'b0;
                end
            end
        endcase
    endtask
endmodule
