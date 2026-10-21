// The asynchronous receiver of КР580ВВ51А (see kr580vv51.sv), sampling RxD
// on the rising edges of RxC as the chip's CLK sees them (docs/vv51.md §8):
// a falling edge on RxD starts a start bit, checked again at its centre
// (x16, x64), then the data, the parity bit and one stop bit, each at the
// centre of its bit.  After a master reset RxD must be seen high once.
module kr580vv51_rx (
    input  logic       clk,
    input  logic       master_reset,
    input  logic       rise,            // an RxC rising edge, seen at a CLK rise
    input  logic       rxd,
    input  logic [1:0] factor,
    input  logic [1:0] length,
    input  logic       pen, ep,
    input  logic       enable,          // RxE
    output logic       done,            // a character assembled
    output logic [7:0] data,
    output logic       perr, ferr,
    output logic       brk              // break: two all-zero frames with no stop bit
);
    typedef enum logic [1:0] { IDLE, START, BITS } state_t;
    state_t     state;
    logic       armed, line_was;
    logic [6:0] count;                  // RxC rises to the next sample
    logic [3:0] bit_no;                 // the bit being taken (1 = first data bit)
    logic [8:0] shift;                  // data and parity as they came
    logic [1:0] zero_frames;
    logic [6:0] per_bit;
    logic [3:0] n;

    always_comb
        case (factor)
            2'b01:   per_bit = 7'd1;
            2'b10:   per_bit = 7'd16;
            default: per_bit = 7'd64;
        endcase
    assign n = 4'd5 + {2'd0, length};

    // the character from the bits taken: data, then its parity and frame
    task automatic finish(input logic stop);
        logic [7:0] d;
        logic       p;
        d = shift[7:0] & (8'hFF >> (4'd8 - n));
        p = shift[n];
        data <= d;
        perr <= pen && (p != (ep ? ^d : ~^d));
        ferr <= !stop;
        done <= 1'b1;
        if (!stop && d == 8'd0) begin
            if (zero_frames != 2'd2) zero_frames <= zero_frames + 2'd1;
            if (zero_frames != 2'd0) brk <= 1'b1;
        end else
            zero_frames <= 2'd0;
        state <= IDLE;
    endtask

    always_ff @(posedge clk) begin
        done <= 1'b0;
        if (master_reset) begin
            armed <= 1'b0; state <= IDLE; brk <= 1'b0; zero_frames <= '0; line_was <= 1'b0;
        end else if (rise) begin
            line_was <= rxd;
            if (rxd) begin                  // RxD high ends a break at once
                armed <= 1'b1;
                brk   <= 1'b0;
                zero_frames <= '0;
            end
            case (state)
                IDLE:
                    // a falling edge starts a frame; a line held low after
                    // an all-zero frame goes on counting frames (break)
                    if (enable && armed && !rxd && (line_was || zero_frames != 2'd0)) begin
                        state  <= per_bit == 7'd1 ? BITS : START;
                        count  <= per_bit == 7'd1 ? 7'd1 : per_bit >> 1;
                        bit_no <= 4'd1;
                        shift  <= '0;
                    end
                START:
                    if (count != 7'd1) count <= count - 7'd1;
                    else if (rxd) state <= IDLE;            // a false start
                    else begin state <= BITS; count <= per_bit; end
                default:
                    if (count != 7'd1) count <= count - 7'd1;
                    else begin
                        count <= per_bit;
                        if (bit_no == n + {3'd0, pen} + 4'd1)
                            finish(rxd);                    // the stop bit
                        else begin
                            shift[bit_no - 4'd1] <= rxd;
                            bit_no <= bit_no + 4'd1;
                        end
                    end
            endcase
        end
    end
endmodule
