// ns4_video: the НС4 board's picture for MiSTer's video interface (plan
// stage 7c).  The board's video DAC (VT1, VT2 and their resistors, left out
// of the model: docs/analog.md) is replaced by what the bench's camera
// takes (sim/board/run_board.cpp): at each ТС15 H rise the blue, red and
// green levels (СЦ, КЦ, ЗЦ H) and the brightness line (D69.9), a line from
// the ССИ L fall.  A level is 255 bright, 170 dim, as the camera paints it.
//
// The blanking comes on one wire, СГИ L: D21's line blanking and D13's
// frame blanking are open collectors tied together (docs/findings.md).
// MiSTer wants the two apart, so a line is put out a line late, from a
// buffer of two (block memory): by then it is known whether any sample of
// it was unblanked - a line with none is the frame's blanking.
//
// Out, a pixel enable (ce_pix, one master tick) for each sample, with the
// colour (0 while blanked), the line and frame syncs (ССИ L, КСИ L) and the
// blankings, all active high, as the line before carried them.
module ns4_video (
    input  logic       clk,
    input  logic       ts15,        // ТС15 H: a sample at each rise
    input  logic       blue,        // СЦ H
    input  logic       red,         // КЦ H
    input  logic       green,       // ЗЦ H
    input  logic       bright,      // D69.9
    input  logic       ssi_n,       // ССИ L, the line sync
    input  logic       ksi_n,       // КСИ L, the frame sync
    input  logic       sgi_n,       // СГИ L, the blanking
    output logic       ce_pix,
    output logic [7:0] r,
    output logic [7:0] g,
    output logic [7:0] b,
    output logic       hs,
    output logic       vs,
    output logic       hblank,
    output logic       vblank
);
    localparam int W = 7;           // a sample: sgi_n ksi_n ssi_n bright green red blue

    logic [W-1:0] lines [2048];     // two lines of up to 1024 samples
    logic [9:0]   x;                // the sample of the line coming in
    logic         bank;             // the line coming in is written here
    logic         seen;             // an unblanked sample in the line coming in
    logic         frame_blank;      // ... in the line going out: none
    logic         ts15_was, ssi_was;
    logic         take;             // a sample now (its stored one read)
    logic [W-1:0] q;

    // the samples come in: a line from the ССИ L fall, a sample at each
    // ТС15 H rise (the fall and the rise of one tick: the line first)
    always_ff @(posedge clk) begin
        ts15_was <= ts15;
        ssi_was  <= ssi_n;
        take     <= 1'b0;
        if (!ssi_n && ssi_was) begin
            bank        <= !bank;
            frame_blank <= !seen;
            seen        <= 1'b0;
            x           <= '0;
        end
        if (ts15 && !ts15_was) begin
            take <= 1'b1;
            if (sgi_n)
                seen <= 1'b1;
            if (!(!ssi_n && ssi_was))
                x <= x + 1'b1;
            else
                x <= 10'd1;
        end
    end

    // the sample written into this line's bank, the line before's read at
    // the same place (block memory: write and read on the clock)
    logic [10:0] at;
    assign at = {bank ^ (!ssi_n && ssi_was), (!ssi_n && ssi_was) ? 10'd0 : x};
    always_ff @(posedge clk)
        if (ts15 && !ts15_was)
            lines[at] <= {sgi_n, ksi_n, ssi_n, bright, green, red, blue};
    always_ff @(posedge clk)
        if (ts15 && !ts15_was)
            q <= lines[{!at[10], at[9:0]}];

    function automatic logic [7:0] level(logic on, logic bright_, logic blank);
        return blank || !on ? 8'd0 : bright_ ? 8'd255 : 8'd170;
    endfunction

    always_ff @(posedge clk) begin
        ce_pix <= take;
        if (take) begin
            b      <= level(q[0], q[3], !q[6]);
            r      <= level(q[1], q[3], !q[6]);
            g      <= level(q[2], q[3], !q[6]);
            hs     <= !q[4];
            vs     <= !q[5];
            hblank <= !q[6];
            vblank <= frame_blank;
        end
    end
endmodule
