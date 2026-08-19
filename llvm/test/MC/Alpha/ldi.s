# RUN: llvm-mc -triple=alpha-unknown-linux-gnu -show-encoding %s | FileCheck %s
# RUN: not llvm-mc -triple=alpha-unknown-linux-gnu --defsym ERR=1 %s 2>&1 \
# RUN:   | FileCheck %s --check-prefix=ERR

# CHECK: lda $0, 5
	ldi $0, 5
# CHECK: lda $1, -7
	ldiq $1, -7
# CHECK:      ldah $2, 4660($31)
# CHECK-NEXT: lda $2, 22136($2)
	ldi $2, 0x12345678
# CHECK:      ldah $3, -8530($31)
# CHECK-NEXT: lda $3, -16657($3)
# CHECK-NEXT: zapnot $3, 15, $3
	ldi $3, 0xdeadbeef
# A single-bit value above 2^32: build 1024 and shift it up by 32.
# CHECK:      lda $4, 1024($31)
# CHECK-NEXT: sll $4, 32, $4
	ldi $4, 0x40000000000
# A full 48-bit value: high half, shift, low half.
# CHECK:      ldah $5, 1($31)
# CHECK-NEXT: lda $5, -8530($5)
# CHECK-NEXT: sll $5, 32, $5
# CHECK-NEXT: ldah $5, -16656($5)
# CHECK-NEXT: lda $5, -13570($5)
	ldi $5, 0xdeadbeefcafe
## The pseudo materializes a constant, so a symbol has no meaning here and is
## refused rather than assembled into something.
.ifdef ERR
# ERR: error: unrecognized instruction mnemonic
	ldi $0, sym
# ERR: error: unrecognized instruction mnemonic
	ldiq $0, sym
.endif
