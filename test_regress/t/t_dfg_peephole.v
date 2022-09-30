// DESCRIPTION: Verilator: Verilog Test module
//
// This file ONLY is placed under the Creative Commons Public Domain, for
// any use, without warranty, 2022 by Geza Lore.
// SPDX-License-Identifier: CC0-1.0

`define signal(name, expr) wire [$bits(expr)-1:0] ``name = expr;

module t (
`include "portlist.vh" // Boilerplate generated by t_dfg_peephole.pl
          rand_a, rand_b
          );

`include "portdecl.vh" // Boilerplate generated by t_dfg_peephole.pl

   input rand_a;
   input rand_b;
   wire [63:0] rand_a;
   wire [63:0] rand_b;

   wire        logic randbit_a = rand_a[0];
   wire        logic [127:0] rand_ba = {rand_b, rand_a};
   wire        logic [127:0] rand_aa = {2{rand_a}};
   wire        logic [63:0] const_a;
   wire        logic [63:0] const_b;
   wire        logic [63:0] array [3:0];
   assign array[0] = (rand_a << 32) | (rand_a >> 32);
   assign array[1] = (rand_a << 16) | (rand_a >> 48);

   // 64 bit all 0 but don't tell V3Const
`define ZERO (const_a & ~const_a)
   // 64 bit all 1 but don't tell V3Const
`define ONES (const_a | ~const_a)
   // x, but in a way only DFG understands
`define DFG(x) ((|`ONES) ? (x) : (~x))

   `signal(SWAP_CONST_IN_COMMUTATIVE_BINARY, rand_a + const_a);
   `signal(SWAP_NOT_IN_COMMUTATIVE_BINARY, rand_a + ~rand_a);
   `signal(SWAP_VAR_IN_COMMUTATIVE_BINARY, rand_b + rand_a);
   `signal(PUSH_BITWISE_OP_THROUGH_CONCAT, 32'h12345678 ^ {8'h0, rand_a[23:0]});
   `signal(PUSH_BITWISE_OP_THROUGH_CONCAT_2, 32'h12345678 ^ {rand_b[7:0], rand_a[23:0]});
   `signal(PUSH_COMPARE_OP_THROUGH_CONCAT, 4'b1011 == {2'b10, rand_a[1:0]});
   `signal(REMOVE_WIDTH_ONE_REDUCTION, &`DFG(rand_a[0]));
   `signal(PUSH_REDUCTION_THROUGH_COND_WITH_CONST_BRANCH, |(rand_a[32] ? rand_a[3:0] : 4'h0));
   `signal(REPLACE_REDUCTION_OF_CONST_AND, &const_a);
   `signal(REPLACE_REDUCTION_OF_CONST_OR,  |const_a);
   `signal(REPLACE_REDUCTION_OF_CONST_XOR, ^const_a);
   `signal(REPLACE_EXTEND, 4'(rand_a[0]));
   `signal(PUSH_NOT_THROUGH_COND, ~(rand_a[0] ? rand_a[4:0] : 5'hb));
   `signal(REMOVE_NOT_NOT, ~`DFG(~`DFG(rand_a)));
   `signal(REPLACE_NOT_NEQ, ~`DFG(rand_a != rand_b));
   `signal(REPLACE_NOT_EQ, ~`DFG(rand_a == rand_b));
   `signal(REPLACE_NOT_OF_CONST, ~4'd0);
   `signal(REPLACE_AND_OF_NOT_AND_NOT, ~rand_a[0] & ~rand_b[0]);
   `signal(REPLACE_AND_OF_NOT_AND_NEQ, ~rand_a[0] & (rand_b != 64'd2));
   `signal(REPLACE_AND_OF_CONST_AND_CONST, const_a & const_b);
   `signal(REPLACE_AND_WITH_ZERO, `ZERO & rand_a);
   `signal(REMOVE_AND_WITH_ONES, `ONES & rand_a);
   `signal(REPLACE_CONTRADICTORY_AND, rand_a & ~rand_a);
   `signal(REPLACE_OR_OF_NOT_AND_NOT, ~rand_a[0] | ~rand_b[0]);
   `signal(REPLACE_OR_OF_NOT_AND_NEQ, ~rand_a[0] | (rand_b != 64'd2));
   `signal(REPLACE_OR_OF_CONCAT_ZERO_LHS_AND_CONCAT_RHS_ZERO, {2'd0, rand_a[1:0]} | {rand_b[1:0], 2'd0});
   `signal(REPLACE_OR_OF_CONCAT_LHS_ZERO_AND_CONCAT_ZERO_RHS, {rand_a[1:0], 2'd0} | {2'd0, rand_b[1:0]});
   `signal(REPLACE_OR_OF_CONST_AND_CONST, const_a | const_b);
   `signal(REMOVE_OR_WITH_ZERO, `ZERO | rand_a);
   `signal(REPLACE_OR_WITH_ONES, `ONES | rand_a);
   `signal(REPLACE_TAUTOLOGICAL_OR, rand_a | ~rand_a);
   `signal(REMOVE_SUB_ZERO, rand_a - `ZERO);
   `signal(REPLACE_SUB_WITH_NOT, rand_a[0] - 1'b1);
   `signal(REMOVE_REDUNDANT_ZEXT_ON_RHS_OF_SHIFT, rand_a << {2'b0, rand_a[2:0]});
   `signal(REPLACE_EQ_OF_CONST_AND_CONST, 4'd0 == 4'd1);
   `signal(REMOVE_FULL_WIDTH_SEL, rand_a[63:0]);
   `signal(REMOVE_SEL_FROM_RHS_OF_CONCAT, rand_ba[63:0]);
   `signal(REMOVE_SEL_FROM_LHS_OF_CONCAT, rand_ba[127:64]);
   `signal(PUSH_SEL_THROUGH_CONCAT, rand_ba[120:0]);
   `signal(PUSH_SEL_THROUGH_REPLICATE, rand_aa[0]);
   `signal(REPLACE_SEL_FROM_CONST, const_a[2]);
   `signal(REPLACE_CONCAT_OF_CONSTS, {const_a, const_b});
   `signal(REPLACE_NESTED_CONCAT_OF_CONSTS_ON_RHS, {`DFG({rand_a, const_a}), const_b});
   `signal(REPLACE_NESTED_CONCAT_OF_CONSTS_ON_LHS, {const_a, `DFG({const_b, rand_a})});
   `signal(REPLACE_CONCAT_ZERO_AND_SEL_TOP_WITH_SHIFTR, {62'd0, rand_a[63:62]});
   `signal(REPLACE_CONCAT_SEL_BOTTOM_AND_ZERO_WITH_SHIFTL, {rand_a[1:0], 62'd0});
   `signal(PUSH_CONCAT_THROUGH_NOTS, {~rand_a, ~rand_b} );
   `signal(REMOVE_CONCAT_OF_ADJOINING_SELS, {`DFG(rand_a[10:3]), `DFG(rand_a[2:1])});
   `signal(REPLACE_NESTED_CONCAT_OF_ADJOINING_SELS_ON_LHS, {rand_a[10:3], {rand_a[2:1], rand_b}});
   `signal(REPLACE_NESTED_CONCAT_OF_ADJOINING_SELS_ON_RHS, {`DFG({rand_b, rand_a[10:3]}), rand_a[2:1]});
   `signal(REMOVE_COND_WITH_FALSE_CONDITION, &`ZERO ? rand_a : rand_b);
   `signal(REMOVE_COND_WITH_TRUE_CONDITION, |`ONES ? rand_a : rand_b);
   `signal(SWAP_COND_WITH_NOT_CONDITION, (~rand_a[0] & |`ONES) ? rand_a : rand_b);
   `signal(SWAP_COND_WITH_NEQ_CONDITION, rand_b != rand_a ? rand_a : rand_b);
   `signal(PULL_NOTS_THROUGH_COND, rand_a[0] ? ~rand_a[4:0] : ~rand_b[4:0]);
   `signal(REPLACE_COND_WITH_THEN_BRANCH_ZERO, rand_a[0] ? |`ZERO : rand_a[1]);
   `signal(REPLACE_COND_WITH_THEN_BRANCH_ONES, rand_a[0] ? |`ONES : rand_a[1]);
   `signal(REPLACE_COND_WITH_ELSE_BRANCH_ZERO, rand_a[0] ? rand_a[1] : |`ZERO);
   `signal(REPLACE_COND_WITH_ELSE_BRANCH_ONES, rand_a[0] ? rand_a[1] : |`ONES);
   `signal(INLINE_ARRAYSEL, array[0]);
   `signal(PUSH_BITWISE_THROUGH_REDUCTION_AND, (&rand_a) & (&rand_b));
   `signal(PUSH_BITWISE_THROUGH_REDUCTION_OR,  (|rand_a) | (|rand_b));
   `signal(PUSH_BITWISE_THROUGH_REDUCTION_XOR, (^rand_a) ^ (^rand_b));
   `signal(PUSH_REDUCTION_THROUGH_CONCAT_AND, &`DFG({rand_a, rand_b}));
   `signal(PUSH_REDUCTION_THROUGH_CONCAT_OR,  |`DFG({rand_a, rand_b}));
   `signal(PUSH_REDUCTION_THROUGH_CONCAT_XOR, ^`DFG({rand_a, rand_b}));
   `signal(REMOVE_WIDTH_ONE_REDUCTION_AND, &`DFG({randbit_a, rand_b}));
   `signal(REMOVE_WIDTH_ONE_REDUCTION_OR,  |`DFG({randbit_a, rand_b}));
   `signal(REMOVE_WIDTH_ONE_REDUCTION_XOR, ^`DFG({randbit_a, rand_b}));
   `signal(REMOVE_XOR_WITH_ZERO, `ZERO ^ rand_a);
   `signal(REMOVE_XOR_WITH_ONES, `ONES ^ rand_a);
   `signal(REPLACE_COND_DEC, randbit_a ? rand_b - 64'b1 : rand_b);
   `signal(REPLACE_COND_INC, randbit_a ? rand_b + 64'b1 : rand_b);
   `signal(RIGHT_LEANING_ASSOC, (((rand_a + rand_b) + rand_a) + rand_b));
   `signal(RIGHT_LEANING_CONCET, {{{rand_a, rand_b}, rand_a}, rand_b});

   // Some selects need extra temporaries
   wire [63:0] sel_from_cond = rand_a[0] ? rand_a : const_a;
   wire [63:0] sel_from_shiftl = rand_a << 10;
   wire [31:0] sel_from_sel = rand_a[10+:32];

   `signal(PUSH_SEL_THROUGH_COND, sel_from_cond[2]);
   `signal(PUSH_SEL_THROUGH_SHIFTL, sel_from_shiftl[20:0]);
   `signal(REPLACE_SEL_FROM_SEL, sel_from_sel[4:3]);

   // Sel from not requires the operand to have a sinle sink, so can't use
   // the chekc due to the raw expression referencing the operand
   wire [63:0] sel_from_not_tmp = ~(rand_a >> rand_b[2:0] << rand_a[3:0]);
   wire        sel_from_not = sel_from_not_tmp[2];
   always @(posedge randbit_a) if ($c(0)) $display(sel_from_not); // Do not remove signal

   // Assigned at the end to avoid inlining by other passes
   assign const_a =  (rand_a | ~rand_a) & 64'h0123456789abcdef;
   assign const_b = ~(rand_a & ~rand_a) & 64'h98badefc10325647;
endmodule
