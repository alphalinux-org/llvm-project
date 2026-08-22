# RUN: llvm-mc -triple=alpha-unknown-linux-gnu -mcpu=ev67 --show-encoding %s \
# RUN:   | FileCheck %s
# RUN: llvm-mc -triple=alpha-unknown-linux-gnu -mcpu=ev67 -filetype=obj %s -o - \
# RUN:   | llvm-objdump -d --mcpu=ev67 - | FileCheck %s --check-prefix=DIS

## The operate instructions the compiler never selects.  Every encoding below
## is what GNU as 2.46.1 produces for the same line, and every one of them
## appears in the platform's own libraries: leaving them out left words that
## assembled but did not disassemble, and words GNU as accepted that this
## assembler rejected.
##
## The trapping-on-overflow arithmetic, which is what GCC's -ftrapv emits.
# CHECK: encoding: [0x03,0x08,0x22,0x40]
# DIS: addl/v $1, $2, $3
	addl/v $1, $2, $3
# CHECK: encoding: [0x23,0x09,0x22,0x40]
# DIS: subl/v $1, $2, $3
	subl/v $1, $2, $3
# CHECK: encoding: [0x03,0x0c,0x22,0x40]
# DIS: addq/v $1, $2, $3
	addq/v $1, $2, $3
# CHECK: encoding: [0x23,0x0d,0x22,0x40]
# DIS: subq/v $1, $2, $3
	subq/v $1, $2, $3
# CHECK: encoding: [0x03,0x08,0x22,0x4c]
# DIS: mull/v $1, $2, $3
	mull/v $1, $2, $3
# CHECK: encoding: [0x03,0x0c,0x22,0x4c]
# DIS: mulq/v $1, $2, $3
	mulq/v $1, $2, $3
# CHECK: encoding: [0x03,0xb8,0x2a,0x40]
# DIS: addl/v $1, 85, $3
	addl/v $1, 85, $3
# CHECK: encoding: [0x23,0xb9,0x2a,0x40]
# DIS: subl/v $1, 85, $3
	subl/v $1, 85, $3
# CHECK: encoding: [0x03,0xbc,0x2a,0x40]
# DIS: addq/v $1, 85, $3
	addq/v $1, 85, $3
# CHECK: encoding: [0x23,0xbd,0x2a,0x40]
# DIS: subq/v $1, 85, $3
	subq/v $1, 85, $3
# CHECK: encoding: [0x03,0xb8,0x2a,0x4c]
# DIS: mull/v $1, 85, $3
	mull/v $1, 85, $3
# CHECK: encoding: [0x03,0xbc,0x2a,0x4c]
# DIS: mulq/v $1, 85, $3
	mulq/v $1, 85, $3

## The literal forms of the logical and multiply-high operations: an operate
## instruction takes a literal wherever it takes $Rb.

# CHECK: encoding: [0x03,0xb5,0x2a,0x44]
# DIS: ornot $1, 85, $3
	ornot $1, 85, $3
# CHECK: encoding: [0x03,0xb9,0x2a,0x44]
# DIS: eqv $1, 85, $3
	eqv $1, 85, $3
# CHECK: encoding: [0x03,0xb6,0x2a,0x4c]
# DIS: umulh $1, 85, $3
	umulh $1, 85, $3

## And of the motion-video minimum and maximum operations.

# CHECK: encoding: [0x03,0xb7,0x2a,0x70]
# DIS: minsb8 $1, 85, $3
	minsb8 $1, 85, $3
# CHECK: encoding: [0x23,0xb7,0x2a,0x70]
# DIS: minsw4 $1, 85, $3
	minsw4 $1, 85, $3
# CHECK: encoding: [0x43,0xb7,0x2a,0x70]
# DIS: minub8 $1, 85, $3
	minub8 $1, 85, $3
# CHECK: encoding: [0x63,0xb7,0x2a,0x70]
# DIS: minuw4 $1, 85, $3
	minuw4 $1, 85, $3
# CHECK: encoding: [0x83,0xb7,0x2a,0x70]
# DIS: maxub8 $1, 85, $3
	maxub8 $1, 85, $3
# CHECK: encoding: [0xa3,0xb7,0x2a,0x70]
# DIS: maxuw4 $1, 85, $3
	maxuw4 $1, 85, $3
# CHECK: encoding: [0xc3,0xb7,0x2a,0x70]
# DIS: maxsb8 $1, 85, $3
	maxsb8 $1, 85, $3
# CHECK: encoding: [0xe3,0xb7,0x2a,0x70]
# DIS: maxsw4 $1, 85, $3
	maxsw4 $1, 85, $3

## The floating conditional moves other than fcmovne, and the copy of a sign
## and exponent.

# CHECK: encoding: [0x43,0x05,0x22,0x5c]
# DIS: fcmoveq $f1, $f2, $f3
	fcmoveq $f1, $f2, $f3
# CHECK: encoding: [0x83,0x05,0x22,0x5c]
# DIS: fcmovlt $f1, $f2, $f3
	fcmovlt $f1, $f2, $f3
# CHECK: encoding: [0xa3,0x05,0x22,0x5c]
# DIS: fcmovge $f1, $f2, $f3
	fcmovge $f1, $f2, $f3
# CHECK: encoding: [0xc3,0x05,0x22,0x5c]
# DIS: fcmovle $f1, $f2, $f3
	fcmovle $f1, $f2, $f3
# CHECK: encoding: [0xe3,0x05,0x22,0x5c]
# DIS: fcmovgt $f1, $f2, $f3
	fcmovgt $f1, $f2, $f3
# CHECK: encoding: [0x43,0x04,0x22,0x5c]
# DIS: cpyse $f1, $f2, $f3
	cpyse $f1, $f2, $f3

## The longword form of an integer held in a floating register.  cvtql takes an
## integer-overflow qualifier and no rounding letter.

# CHECK: encoding: [0x02,0x02,0xe1,0x5f]
# DIS: cvtlq $f1, $f2
	cvtlq $f1, $f2
# CHECK: encoding: [0x02,0x06,0xe1,0x5f]
# DIS: cvtql $f1, $f2
	cvtql $f1, $f2
# CHECK: encoding: [0x02,0x26,0xe1,0x5f]
# DIS: cvtql/v $f1, $f2
	cvtql/v $f1, $f2
# CHECK: encoding: [0x02,0xa6,0xe1,0x5f]
# DIS: cvtql/sv $f1, $f2
	cvtql/sv $f1, $f2

## The unordered compare, which is how a NaN test is written.

# CHECK: encoding: [0x83,0x14,0x22,0x58]
# DIS: cmptun $f1, $f2, $f3
	cmptun $f1, $f2, $f3

## The conditional moves for the two conditions DAG canonicalization rewrites
## away, so nothing selects them.

# CHECK: encoding: [0xc3,0x08,0x22,0x44]
# DIS: cmovge $1, $2, $3
	cmovge $1, $2, $3
# CHECK: encoding: [0x83,0x0c,0x22,0x44]
# DIS: cmovle $1, $2, $3
	cmovle $1, $2, $3

## And the fourth memory-format branch, which swaps to a coroutine.

# CHECK: encoding: [0x00,0xc0,0x5b,0x6b]
# DIS: jsr_coroutine $26, ($27)
	jsr_coroutine $26, ($27)
