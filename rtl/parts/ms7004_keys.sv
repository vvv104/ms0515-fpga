// ms7004_keys: a PC keyboard on the МС 7004's matrix, for the MiSTer core
// - MiSTer's ps2_key in, the matrix's pressed keys out (rtl/parts/
// ms7004_matrix.sv puts them on the return lines).  The keyboard itself is
// the МС 7004 (rtl/board/ms7004_machine.sv), its own firmware sending the
// codes; this only presses its keys.  The tables: ms7004_keys_pkg
// (tools/ms7004_keys.py, from netlist/ms7004/pc_keys.csv).
//
// Two modes, the menu's.  By position (mode 1): a PC key presses the
// МС 7004 key where it stands, Shift and all, as long as it is held - for
// programs and games that read keys.  By character (mode 0): a PC key gives
// the character on its cap - the МС 7004 key that has it, its ВР pressed or
// let go as the character needs (an «@», Shift+2 on a PC, is Ю/@ unshifted
// in ЛАТ), and РУС/ЛАТ turned over and back for a character the mode at hand
// lacks.  Which mode the machine is in the translator reads off the
// keyboard's ЛАТ indicator (`rus`: dark), which the computer sets.  The
// modifiers - СУ, КМП, ФКС, РУС/ЛАТ - pass by position in both modes, the
// PC's Shift as the МС 7004's ВР between characters.
//
// The firmware scans its matrix every 3.5 ms and takes a key after 2 ms
// more for its bounce (ТО 5.3): each step of a character - РУС/ЛАТ, ВР, the
// key - holds STEP ticks, two scans and more; keys typed meanwhile wait in
// a queue.  A key held is held: the firmware repeats it.
module ms7004_keys
    import ms7004_keys_pkg::*;
#(
    parameter int STEP = 2_400_000      // 20 ms of the 120 MHz master clock
) (
    input  logic         clk,
    input  logic [10:0]  ps2_key,       // [10] toggles each event, [9] pressed, [8] extended, [7:0] the code
    input  logic         mode,          // 0 by character, 1 by position
    input  logic         rus,           // the machine in РУС mode (the ЛАТ indicator dark)
    output logic [127:0] pressed
);
    // ---- the events, queued -------------------------------------------------
    logic       toggle_was;
    logic [9:0] fifo [8];               // {pressed, code}
    logic [2:0] head, tail;
    logic       empty;
    logic [9:0] ev;                     // the head as it stands now
    logic [8:0] ev_q;                   // the head's code a tick ago: what the tables look up
    assign empty = head == tail;
    assign ev = fifo[head];

    // the modifiers, by position in both modes: the Shifts, Ctrl, the Alts, Caps
    function automatic logic modifier(logic [8:0] code);
        return code == 9'h012 || code == 9'h059 || code == 9'h014 || code == 9'h011 ||
               code == 9'h111 || code == 9'h058;
    endfunction

    // ---- the keys held by position, the PC's Shift ---------------------------------
    logic [127:0] held;
    logic         shift_l, shift_r, pc_shift;
    assign pc_shift = shift_l || shift_r;

    // ---- a character, step by step ----------------------------------------------------
    typedef enum logic [3:0] {
        IDLE, LAT_PRESS, LAT_RELEASE, SET_SHIFT, PRESS, HOLD, RELEASE, BACK_PRESS, BACK_RELEASE
    } step_t;
    step_t       st;
    logic [22:0] wait_n;
    logic [8:0]  cur;                   // the PC key being typed
    logic [6:0]  place;
    logic [1:0]  need;                  // ВР: 0 as the PC's Shift, 1 let go, 2 pressed
    logic        lat;                   // РУС/ЛАТ to turn over for it
    logic        ms_shift, key_down, lat_down;
    // the tables looked up for the queue's head two ticks late (they are
    // large, and the queue's read before them misses 120 MHz in one): the
    // head is registered, then looked up; after taking an event the steps
    // wait two ticks for the next
    logic [10:0] found;
    logic [7:0]  pos;
    logic        stood, looked;         // the head has stood a tick, two: found and pos are its

    always_ff @(posedge clk) begin
        ev_q <= ev[8:0];
        found <= character(ev_q, pc_shift, rus);
        pos <= position(ev_q);
        stood <= !empty;
        looked <= stood && !empty;
        toggle_was <= ps2_key[10];
        if (ps2_key[10] != toggle_was && tail + 3'd1 != head) begin
            fifo[tail] <= {ps2_key[9], ps2_key[8:0]};
            tail <= tail + 3'd1;
        end
        if (wait_n != '0)
            wait_n <= wait_n - 1'b1;
        else
            step();
    end

    // a key by position: the matrix's key where the PC key stands; the PC's
    // Shifts tracked, and in mode 0 put on ВР only between characters
    task automatic by_position(logic [9:0] e, logic [7:0] p);
        if (e[8:0] == 9'h012) shift_l <= e[9];
        if (e[8:0] == 9'h059) shift_r <= e[9];
        if (p[7] && (mode || (e[8:0] != 9'h012 && e[8:0] != 9'h059)))
            held[p[6:0]] <= e[9];
        if (!mode && (e[8:0] == 9'h012 || e[8:0] == 9'h059) && st == IDLE)
            ms_shift <= e[9] || (e[8:0] == 9'h012 ? shift_r : shift_l);
    endtask

    // the queue's head taken: two ticks for the tables' lookup of the next
    task automatic next_event();
        head <= head + 3'd1;
        wait_n <= 23'd2;
    endtask

    task automatic pause(step_t next);
        wait_n <= 23'(STEP);
        st <= next;
    endtask

    task automatic step();
        case (st)
            IDLE: if (!empty && looked) begin
                next_event();
                if (mode || modifier(ev[8:0])) begin
                    by_position(ev, pos);
                end else if (ev[9] && found[10]) begin
                    cur <= ev[8:0];
                    place <= found[9:3];
                    need <= found[2:1];
                    lat <= found[0];
                    st <= found[0] ? LAT_PRESS : SET_SHIFT;
                end
            end
            LAT_PRESS: begin lat_down <= 1'b1; pause(LAT_RELEASE); end
            LAT_RELEASE: begin lat_down <= 1'b0; pause(SET_SHIFT); end
            SET_SHIFT: begin
                ms_shift <= need == 2'd0 ? pc_shift : need == 2'd2;
                pause(PRESS);
            end
            PRESS: begin key_down <= 1'b1; pause(HOLD); end
            HOLD: if (!empty && looked) begin
                if (ev[8:0] == cur && !ev[9]) begin               // let go
                    head <= head + 3'd1;
                    key_down <= 1'b0;
                    pause(RELEASE);
                end else if (modifier(ev[8:0])) begin
                    next_event();
                    by_position(ev, pos);
                end else if (!ev[9]) begin                        // another key let go
                    next_event();
                end else begin                                    // another key: this one up first
                    key_down <= 1'b0;
                    pause(RELEASE);
                end
            end
            RELEASE: begin ms_shift <= pc_shift; pause(lat ? BACK_PRESS : IDLE); end
            BACK_PRESS: begin lat_down <= 1'b1; pause(BACK_RELEASE); end
            BACK_RELEASE: begin lat_down <= 1'b0; lat <= 1'b0; pause(IDLE); end
            default: st <= IDLE;
        endcase
    endtask

    always_comb begin
        pressed = held;
        if (key_down) pressed[place] = 1'b1;
        if (lat_down) pressed[RUS_LAT] = 1'b1;
        if (!mode && ms_shift) pressed[SHIFT_L] = 1'b1;
    end
endmodule
