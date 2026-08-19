# RUN: llvm-mc -triple=alpha-unknown-linux-gnu -filetype=obj %s -o %t.o
# RUN: llvm-objdump -d --triple=alpha-unknown-linux-gnu %t.o | FileCheck %s

## The literal forms of the scaled add/subtract instructions, and the two
## conditional moves that test the low bit of a register.  Codegen selects only
## the register forms of the first group and neither of the second, so these are
## reached from hand-written assembly.  Encodings match GNU as 2.46.1.

# CHECK: s4addl $16, 1, $0
	s4addl $16, 1, $0
# CHECK: s8addl $16, 2, $0
	s8addl $16, 2, $0
# CHECK: s4subl $16, 3, $0
	s4subl $16, 3, $0
# CHECK: s8subl $16, 4, $0
	s8subl $16, 4, $0
# CHECK: s4addq $16, 5, $0
	s4addq $16, 5, $0
# CHECK: s8addq $16, 6, $0
	s8addq $16, 6, $0
# CHECK: s4subq $16, 7, $0
	s4subq $16, 7, $0
# CHECK: s8subq $16, 8, $0
	s8subq $16, 8, $0

# CHECK: cmovlbs $16, $17, $0
	cmovlbs $16, $17, $0
# CHECK: cmovlbc $16, $17, $0
	cmovlbc $16, $17, $0
