// К555ИЕ10 (74LS161): 4-bit binary counter, rising clock (pin 2);
// synchronous load while LOAD (pin 9) is low, asynchronous clear while CLR
// (pin 1) is low, counts while ENP (pin 7) and ENT (pin 10) are high;
// RCO (pin 15) = ENT and the count at 15.
module k555ie10 (
    input  logic clk,
    input  logic p1,  input  logic p2,  input  logic p9,           // CLR C LOAD
    input  logic p7,  input  logic p10,                            // ENP ENT
    input  logic p3,  input  logic p4,  input  logic p5,  input  logic p6,   // A..D
    output logic p14, output logic p13, output logic p12, output logic p11,  // QA..QD
    output logic p15                                               // RCO
);
    logic [3:0] count, q;
    logic       c_was;

    always_ff @(posedge clk) begin
        c_was <= p2;
        if (!p1)
            count <= '0;
        else if (p2 && !c_was) begin
            if (!p9)
                count <= {p6, p5, p4, p3};
            else if (p7 && p10)
                count <= count + 4'd1;
        end
    end
    assign q = count;
    assign {p11, p12, p13, p14} = q;
    assign p15 = p10 & (q == 4'hF);
endmodule
