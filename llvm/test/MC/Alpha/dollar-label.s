# RUN: llvm-mc -triple=alpha-unknown-linux-gnu -filetype=obj %s -o %t.o
# RUN: llvm-objdump -dr %t.o | FileCheck %s

# GNU as uses '$' for both registers ($0-$31) and local labels ($name).  A
# branch to a '$'-prefixed label must assemble to the branch, not be dropped as
# a malformed register operand.

# CHECK-LABEL: <f>:
# CHECK: bne $1, {{.*}}
# CHECK: beq $2, {{.*}}
# CHECK: br {{.*}}
f:
	bne $1, $target
	beq $2, $target
	br $target
	nop
$target:
	ret ($26)

# A '$'-label also works inside a displacement expression, as the kernel's
# exception-table macros use ($exception - <local>b)($reg); the same-section
# label difference folds to a constant displacement.
# CHECK-LABEL: <g>:
# CHECK: stq_u $31, 0($16)
# CHECK: lda $31, {{-?[0-9]+}}($16)
g:
99:	stq_u $31, 0($16)
	lda $31, $exception-99b($16)
$exception:
	ret ($26)
