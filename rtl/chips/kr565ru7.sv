// КР565РУ7 (41256): a 256K x 1 dynamic RAM, the memory expansion module's
// (plan 6a).  КР565РУ5 with a ninth address pin, A8 (pin 1), in the place
// that is free on the 4164 (eandc К565РУ7 sheet p.1-2): -RAS falling (pin 4)
// takes the row, -CAS falling (pin 15) the column; with -WE (pin 3) low by
// then the cycle is an early write of DI (pin 2) and DO stays off, else a
// read: DO (pin 14) shows the cell while -CAS is low, and -WE falling later
// writes DI there.  DO is controlled by -CAS alone, as КР565РУ5's (see
// kr565ru5.sv for why).
//
// Strobes are seen on the master clock, the row the tick before -RAS fell,
// the column as -CAS falls - as КР565РУ5's.  The cells do not decay:
// refresh is not modelled.
module kr565ru7 (
    input  logic clk,
    input  logic p2,                                                   // DI
    input  logic p3,  input  logic p4,  input  logic p15,              // -WE -RAS -CAS
    input  logic p5,  input  logic p7,  input  logic p6,  input  logic p12,   // A0..A3
    input  logic p11, input  logic p10, input  logic p13, input  logic p9,    // A4..A7
    input  logic p1,                                                   // A8
    output logic p14, output logic p14_oe                              // DO
);
    logic        cells [262144];
    logic [8:0]  a, a_was, row, column;
    logic        ras_was, cas_was, we_was, reading;

    assign a = {p1, p9, p13, p10, p11, p12, p6, p7, p5};

    always_ff @(posedge clk) begin
        a_was   <= a;
        ras_was <= p4;
        cas_was <= p15;
        we_was  <= p3;
        if (!p4 && ras_was)
            row <= a_was;
        if (p15)
            reading <= 1'b0;
        if (!p4 && !p15 && cas_was) begin
            column <= a;
            if (!p3)
                cells[{row, a}] <= p2;                  // early write
            else begin
                reading <= 1'b1;
                p14     <= cells[{row, a}];
            end
        end else if (!p4 && !p15 && !p3 && we_was)
            cells[{row, column}] <= p2;                 // late write
    end
    assign p14_oe = reading;
endmodule
