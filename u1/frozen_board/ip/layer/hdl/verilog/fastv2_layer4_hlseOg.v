
`timescale 1 ns / 1 ps

  module fastv2_layer4_hlseOg_DSP48_1(a, b, p);
input [10 - 1 : 0] a;
input [9 - 1 : 0] b;
output [40 - 1 : 0] p;

assign p = $unsigned (a) * $unsigned (b);

endmodule
`timescale 1 ns / 1 ps
module fastv2_layer4_hlseOg(
    din0,
    din1,
    dout);

parameter ID = 32'd1;
parameter NUM_STAGE = 32'd1;
parameter din0_WIDTH = 32'd1;
parameter din1_WIDTH = 32'd1;
parameter dout_WIDTH = 32'd1;
input[din0_WIDTH - 1:0] din0;
input[din1_WIDTH - 1:0] din1;
output[dout_WIDTH - 1:0] dout;



fastv2_layer4_hlseOg_DSP48_1 fastv2_layer4_hlseOg_DSP48_1_U(
    .a( din0 ),
    .b( din1 ),
    .p( dout ));

endmodule

