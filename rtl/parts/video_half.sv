// video_half: ns4_video's pixels from the 120 MHz master clock onto a
// 60 MHz video clock for MiSTer's sys/ (its scaler and video measurement
// do not run at 120 MHz).  The two clocks come from one PLL, the slow one
// rising with every other edge of the fast.  A pixel comes every eight
// master ticks at the most: each is held, and a toggle tells the slow side,
// which puts it out with its pixel enable for one of its cycles.
module video_half (
    input  logic       clk,             // 120 MHz
    input  logic       ce_pix,
    input  logic [7:0] r, g, b,
    input  logic       hs, vs, hblank, vblank,
    input  logic       clk_video,       // 60 MHz, in phase with clk
    output logic       ce_out,
    output logic [7:0] r_out, g_out, b_out,
    output logic       hs_out, vs_out, hblank_out, vblank_out
);
    logic [27:0] held;
    logic        toggle;
    always_ff @(posedge clk)
        if (ce_pix) begin
            held <= {r, g, b, hs, vs, hblank, vblank};
            toggle <= !toggle;
        end

    logic seen;
    always_ff @(posedge clk_video) begin
        ce_out <= toggle != seen;
        seen <= toggle;
        if (toggle != seen)
            {r_out, g_out, b_out, hs_out, vs_out, hblank_out, vblank_out} <= held;
    end
endmodule
