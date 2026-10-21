// The asynchronous transmitter of КР580ВВ51А (see kr580vv51.sv): a
// character leaves on the falling edges of TxC, as the chip's CLK sees
// them (docs/vv51.md §7): a start bit, the data LSB first, the parity bit,
// the stop bits - each bit `factor` TxC periods, 1½ stop bits a half more.
module kr580vv51_tx (
    input  logic       clk,
    input  logic       reset,           // master or internal reset: idle
    input  logic       fall,            // a TxC falling edge, seen at a CLK rise
    input  logic [1:0] factor,          // B2B1: 01 x1, 10 x16, 11 x64
    input  logic [1:0] length,          // L2L1: 5..8 bits
    input  logic       pen, ep,
    input  logic [1:0] stops,           // S2S1: 01 1, 10 1½, 11 2
    input  logic       enable,          // TxEN and -CTS low
    input  logic       sbrk,
    input  logic       full,            // a character waits in the buffer
    input  logic [7:0] data,
    output logic       take,            // ... and is taken into the shifter now
    output logic       busy,
    output logic       txd
);
    logic [10:0] frame;                 // start, data, parity, then ones
    logic [3:0]  bit_no, last;          // the bit on the line, the last (stop) bit
    logic [6:0]  ticks;                 // TxC falls left in this bit
    logic [6:0]  per_bit;
    logic        out;

    always_comb
        case (factor)
            2'b01:   per_bit = 7'd1;
            2'b10:   per_bit = 7'd16;
            default: per_bit = 7'd64;
        endcase

    // the stop bits as one bit of 1, 1½ or 2 bit times (1½ at x1 taken as 2)
    function automatic logic [6:0] stop_ticks(input logic [1:0] s, input logic [6:0] p);
        case (s)
            2'b10:   return p == 7'd1 ? 7'd2 : p + (p >> 1);
            2'b11:   return {p[5:0], 1'b0};
            default: return p;
        endcase
    endfunction

    function automatic logic [10:0] framed(input logic [7:0] d, input logic [1:0] l,
                                           input logic pe_on, input logic even);
        logic [7:0] m;
        logic [3:0] n;
        logic       p;
        n = 4'd5 + {2'd0, l};
        m = d & (8'hFF >> (4'd8 - n));
        p = even ? ^m : ~^m;
        // ones above the data and parity are the stop bit
        return ({3'b111, 8'hFF} << (n + {3'd0, pe_on} + 4'd1)) |
               ({1'b0, pe_on & p, 9'h000} >> (4'd8 - n)) |
               {2'b00, m, 1'b0};
    endfunction

    assign txd = sbrk ? 1'b0 : out;

    always_ff @(posedge clk) begin
        take <= 1'b0;
        if (reset) begin
            busy <= 1'b0;
            out  <= 1'b1;
        end else if (fall) begin
            if (busy && ticks != 7'd1)
                ticks <= ticks - 7'd1;
            else if (busy && bit_no != last) begin
                bit_no <= bit_no + 4'd1;
                out    <= frame[bit_no + 4'd1];
                ticks  <= bit_no + 4'd1 == last ? stop_ticks(stops, per_bit) : per_bit;
            end else if (full && enable) begin      // the next character: its start bit
                frame  <= framed(data, length, pen, ep);
                last   <= 4'd6 + {2'd0, length} + {3'd0, pen};
                bit_no <= 4'd0;
                out    <= 1'b0;
                ticks  <= per_bit;
                busy   <= 1'b1;
                take   <= 1'b1;
            end else begin
                busy <= 1'b0;
                out  <= 1'b1;
            end
        end
    end
endmodule
