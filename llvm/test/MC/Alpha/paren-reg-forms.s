# RUN: llvm-mc -triple=alpha-unknown-linux-gnu --show-encoding %s | FileCheck %s
# RUN: not llvm-mc -triple=alpha-unknown-linux-gnu -filetype=obj \
# RUN:   --defsym ERR=1 %s 2>&1 | FileCheck %s --check-prefix=ERR

## The full forms of ret, jsr and jmp that hand-written assembly uses.
## Every encoding below is what GNU as 2.46.1 produces for the same line.

## ret takes its hint literally: it selects a prediction-stack action rather
## than naming a target.
# CHECK: encoding: [0x07,0x80,0x23,0x69]
	ret $9, ($3), 7
# CHECK: encoding: [0x00,0x80,0x23,0x69]
	ret $9, ($3)

## jsr and jmp scale theirs, because it is a displacement to the predicted
## target measured in longwords from the next instruction.
# CHECK: encoding: [0x01,0x40,0x5b,0x6b]
	jsr $26, ($27), 4
# CHECK: encoding: [0x02,0x40,0x5b,0x6b]
	jsr $26, ($27), 8
# CHECK: encoding: [0x03,0x00,0xa3,0x68]
	jmp $5, ($3), 12

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
