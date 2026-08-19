# RUN: llvm-mc -triple=alpha-unknown-linux-gnu --show-encoding %s | FileCheck %s

# Operand forms and mnemonics the kernel's hand-written assembly uses, beyond
# what the code generator emits.

# ldq_u/stq_u take a full disp($base) memory operand.
# CHECK: ldq_u $1, 8($17)                 # encoding: [0x08,0x00,0x31,0x2c]
	ldq_u $1, 8($17)
# CHECK: stq_u $1, 0($16)                 # encoding: [0x00,0x00,0x30,0x3c]
	stq_u $1, 0($16)

# lda with a bare displacement (implicit $31 base).
# CHECK: lda $0, 128                      # encoding: [0x80,0x00,0x1f,0x20]
	lda $0, 128

# Extract/insert/mask, conditional move and amask with a literal operand.
# CHECK: insbl $17, 1, $2                 # encoding: [0x62,0x31,0x20,0x4a]
	insbl $17, 1, $2
# CHECK: cmoveq $22, 63, $3               # encoding: [0x83,0xf4,0xc7,0x46]
	cmoveq $22, 63, $3
# CHECK: amask 4, $0                      # encoding: [0x20,0x9c,0xe0,0x47]
	amask 4, $0

# andnot is an alias of bic.
# CHECK: bic $1, $2, $3                    # encoding: [0x03,0x01,0x22,0x44]
	andnot $1, $2, $3
