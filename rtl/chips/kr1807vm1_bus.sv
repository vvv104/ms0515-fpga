// The bus side of КР1807ВМ1 (see kr1807vm1.sv): one transaction at a
// time on the pins, phase by phase, as docs/t11-model.md places the edges
// (16-bit static bus).  XTL1 is sampled on the master clock: its rising
// edges start the phases, its falling edges the half-phases.  The core
// sets the request; the transaction takes it at the XTL edge it starts on
// (`take`) and hands back the read data and the AI pins it latched.
module kr1807vm1_bus #(
    parameter int PI_FALL_DELAY  = 3,   // master ticks after -CAS rises (tPIP, tNHP)
    parameter int RAS_RISE_DELAY = 2,   // master ticks after the half-phase past -CAS (tPSR)
    parameter int SEL1_RAS_DELAY = 6    // master ticks from SEL1 falling to -RAS rising (tRHS)
) (
    input  logic        clk,
    input  logic        xtl,            // XTL1
    input  logic        run,            // 0 while PUP is high: idle, no transactions
    input  logic        bclr,           // -BCLR asserted: DAL let go for the mode
    // the request, held by the core until `take`
    input  logic [2:0]  req_kind,
    input  logic        req_fetch,
    input  logic [15:0] req_addr,
    input  logic [15:0] req_data,
    input  logic [1:0]  req_bytes,      // write: {high, low} byte enables
    input  logic [4:0]  req_ack,        // IACK: DAL<12:8>
    input  logic        delayed,        // MR8: delayed R/W
    input  logic        cout_pulse,     // MR0: COUT once per microcycle
    output logic        phase,          // one master tick at each XTL rising edge
    output logic        take,           // a transaction begins: the request is taken
    output logic        data_valid,     // read data latched (-CAS trailing edge)
    output logic [15:0] data,
    output logic        ai_valid,       // AI latched (PI trailing edge)
    output logic        ai_requests,    // ... in a read or ASPI: the requests count
    output logic [7:0]  ai,
    output logic        vector_valid,   // IACK: DAL<7:2> latched (SEL1 trailing edge)
    // pins
    input  logic [15:0] dal_in,
    output logic [15:0] dal_out,
    output logic [15:0] dal_oe,
    input  logic [7:0]  ai_in,
    input  logic        ready_n,        // READY: a falling edge asks for one slip
    output logic        ras_n, cas_n, pi, cout, sel0, sel1, rwhb, rwlb,
    output logic        rw_oe           // R/-WHB, R/-WLB driven (not during a DMA grant)
);
    import kr1807vm1_pkg::*;

    logic        xtl_was, rise, fall, ready_was;
    logic [4:0]  half;                  // the half-phase that begins next
    logic [4:0]  halves;                // the transaction's length in half-phases
    logic [2:0]  kind;
    logic [1:0]  bytes;
    logic [15:0] wdata;
    logic [15:0] drive;                 // DAL driven, before -BCLR lets them go
    logic [3:0]  ras_wait;              // master ticks left to -RAS rising
    logic [3:0]  pi_wait;               // master ticks left to PI falling
    logic [2:0]  slips;                 // READY slips asked for and not yet taken
    logic        cout_div;              // COUT in constant mode: fosc/2
    logic        cout_on;               // COUT in pulse mode

    assign rise   = xtl && !xtl_was;
    assign fall   = !xtl && xtl_was;
    assign phase  = rise && run;
    assign cout   = cout_pulse ? cout_on : cout_div;
    assign dal_oe = bclr ? '0 : drive;

    function automatic logic [4:0] length(input logic [2:0] k);
        case (k)
            BUS_READ, BUS_WRITE: return 5'd12;
            BUS_ASPI, BUS_IACK:  return 5'd8;
            BUS_DMG:             return 5'd16;
            default:             return 5'd6;
        endcase
    endfunction

    always_ff @(posedge clk) begin
        xtl_was      <= xtl;
        ready_was    <= ready_n;
        take         <= 1'b0;
        data_valid   <= 1'b0;
        ai_valid     <= 1'b0;
        vector_valid <= 1'b0;
        timers();
        // a READY falling edge while -RAS is low asks for a slip
        if (!ras_n && !ready_n && ready_was && slips != 3'd7)
            slips <= slips + 3'd1;
        if (!run) begin
            half <= '0; halves <= '0; kind <= BUS_NOP; slips <= '0;
            ras_n <= 1'b1; cas_n <= 1'b1; pi <= 1'b0; cout_on <= 1'b0;
            sel0 <= 1'b0; sel1 <= 1'b0; rwhb <= 1'b1; rwlb <= 1'b1; rw_oe <= 1'b1;
            drive <= '0; ras_wait <= '0; pi_wait <= '0; cout_div <= 1'b0;
        end else if (rise || fall) begin
            if (rise && half == halves) begin
                // a new transaction: the request is taken at its φ2
                half   <= 5'd1;
                halves <= length(req_kind);
                kind   <= req_kind;
                bytes  <= req_bytes;
                wdata  <= req_data;
                take   <= 1'b1;
                begin_transaction();
            end else if (half != halves) begin
                half <= half + 5'd1;
                edge_at(half);
            end
            if (rise && !cout_pulse)
                cout_div <= !cout_div;
        end
    end

    // the strobes that end a few master ticks after an XTL edge
    task automatic timers();
        if (ras_wait != 0) begin
            ras_wait <= ras_wait - 4'd1;
            if (ras_wait == 4'd1)
                ras_n <= 1'b1;
        end
        if (pi_wait != 0) begin
            pi_wait <= pi_wait - 4'd1;
            if (pi_wait == 4'd1) begin
                pi          <= 1'b0;
                ai          <= ai_in;
                ai_valid    <= 1'b1;
                ai_requests <= kind == BUS_READ;
            end
        end
    endtask

    // the edges at the start of a transaction (t = 0)
    task automatic begin_transaction();
        rwhb  <= 1'b1;
        rwlb  <= 1'b1;
        rw_oe <= 1'b1;
        sel0  <= 1'b0;
        case (req_kind)
            BUS_READ, BUS_WRITE: begin
                dal_out <= req_addr;
                drive   <= '1;
                sel0    <= req_fetch;
                if (req_kind == BUS_WRITE && !delayed) begin
                    rwhb <= !req_bytes[1];
                    rwlb <= !req_bytes[0];
                end
            end
            BUS_ASPI: begin
                drive <= '0;
                cas_n <= 1'b0;
            end
            BUS_IACK: begin
                dal_out <= {3'b000, req_ack, 8'h00};
                drive   <= 16'hFF00;
                sel1    <= 1'b1;
            end
            BUS_DMG: begin
                drive <= '0;
                rw_oe <= 1'b0;
                sel0  <= 1'b1;
                sel1  <= 1'b1;
            end
            default: ;
        endcase
    endtask

    // the edges at the start of half-phase h (t = h / 2)
    task automatic edge_at(input logic [4:0] h);
        case (kind)
            BUS_READ, BUS_WRITE: rw_edge(h);
            BUS_ASPI:            aspi_edge(h);
            BUS_IACK:            iack_edge(h);
            BUS_DMG:             dmg_edge(h);
            default:             nop_edge(h);
        endcase
    endtask

    task automatic nop_edge(input logic [4:0] h);
        if (h == 5'd2) cout_on <= 1'b1;
        if (h == 5'd4) cout_on <= 1'b0;
    endtask

    // -RAS falls; READY already low there counts as a slip of its own.  A
    // slip replaying this edge finds -RAS low and leaves it.
    task automatic ras_falls();
        if (ras_n) begin
            ras_n <= 1'b0;
            slips <= !ready_n ? 3'd1 : 3'd0;
        end
    endtask

    // A slip at half `from`: one more microcycle (3T) before it, replaying
    // the halves from `to` (COUT pulses again; the other edges are repeats).
    task automatic slip(input logic [4:0] to);
        slips   <= slips - 3'd1;
        half    <= to;
        cout_on <= 1'b0;
    endtask

    task automatic rw_edge(input logic [4:0] h);
        case (h)
            5'd2: begin ras_falls(); cout_on <= 1'b1; end
            5'd4: cout_on <= 1'b0;
            5'd5: begin
                cas_n <= 1'b0;
                if (kind == BUS_WRITE) begin
                    dal_out <= wdata;
                    if (delayed) begin
                        rwhb <= !bytes[1];
                        rwlb <= !bytes[0];
                    end
                end else
                    drive <= '0;
            end
            5'd7: begin pi <= 1'b1; sel0 <= 1'b0; end
            5'd8: cout_on <= 1'b1;
            5'd10:
                if (slips != 0)
                    slip(5'd5);
                else begin
                    cout_on <= 1'b0;
                    cas_n   <= 1'b1;
                    pi_wait <= PI_FALL_DELAY[3:0];
                    if (kind == BUS_READ) begin
                        data       <= dal_in;
                        data_valid <= 1'b1;
                    end else if (delayed) begin
                        rwhb <= 1'b1;
                        rwlb <= 1'b1;
                    end
                end
            5'd11: ras_wait <= RAS_RISE_DELAY[3:0];
            default: ;
        endcase
    endtask

    task automatic aspi_edge(input logic [4:0] h);
        case (h)
            5'd2: pi <= 1'b1;
            5'd4: cout_on <= 1'b1;
            5'd6: begin cout_on <= 1'b0; cas_n <= 1'b1; end
            5'd7: begin
                pi          <= 1'b0;
                ai          <= ai_in;
                ai_valid    <= 1'b1;
                ai_requests <= 1'b1;
            end
            default: ;
        endcase
    endtask

    task automatic iack_edge(input logic [4:0] h);
        case (h)
            5'd2: ras_falls();
            5'd4: cout_on <= 1'b1;
            5'd6: cout_on <= 1'b0;
            5'd7:
                if (slips != 0)
                    slip(5'd2);
                else begin
                    sel1         <= 1'b0;
                    data         <= dal_in;
                    vector_valid <= 1'b1;
                    ras_wait     <= SEL1_RAS_DELAY[3:0];
                end
            default: ;
        endcase
    endtask

    // a DMA grant: φ2 φD φW φ1 φ2 φD φW φ1, the strobes for the master
    task automatic dmg_edge(input logic [4:0] h);
        case (h)
            5'd4: begin ras_falls(); cout_on <= 1'b1; end
            5'd6: cout_on <= 1'b0;
            5'd9: cas_n <= 1'b0;
            5'd11: pi <= 1'b1;
            5'd12: cout_on <= 1'b1;
            5'd14:
                if (slips != 0)
                    slip(5'd9);
                else begin
                    cout_on  <= 1'b0;
                    cas_n    <= 1'b1;
                    sel1     <= 1'b0;
                    pi_wait  <= PI_FALL_DELAY[3:0];
                    ras_wait <= SEL1_RAS_DELAY[3:0];
                end
            default: ;
        endcase
    endtask
endmodule
