# RUN: llvm-mc -triple=alpha-unknown-linux-gnu --show-encoding %s | FileCheck %s
# RUN: not llvm-mc -triple=alpha-unknown-linux-gnu -filetype=obj \
# RUN:   --defsym ERR=1 %s 2>&1 | FileCheck %s --check-prefix=ERR

## The full forms of ldgp, ret, jsr and jmp that hand-written assembly uses.
## Every encoding below is what GNU as 2.46.1 produces for the same line.

## ldgp names its own destination; it is not always $29.  The displacement
## goes in the lda half.
# CHECK: ldah $0, 4($27) !gpdisp
# CHECK: lda $0, 0($0)
	ldgp $0, 0($27)
# CHECK: ldah $29, 4($27) !gpdisp
# CHECK: lda $29, 8($29)
# CHECK-SAME: encoding: [0x08,0x00,0xbd,0x23]
	ldgp $29, 8($27)

## ret takes its hint literally: it selects a prediction-stack action rather
## than naming a target.
# CHECK: encoding: [0x07,0x80,0x23,0x69]
	ret $9, ($3), 7
# CHECK: encoding: [0x00,0x80,0x23,0x69]
	ret $9, ($3)

.ifdef ERR
## A parenthesized register has nowhere to put a displacement, so one written
## anyway has to be refused rather than dropped.  GNU as rejects all three.
# ERR: error: invalid operand for instruction
	jmp 8($3)
# ERR: error: invalid operand for instruction
	ret 16($26)
# ERR: error: invalid operand for instruction
	wh64 32($16)
.endif
