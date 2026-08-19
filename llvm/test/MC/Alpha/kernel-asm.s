# RUN: llvm-mc -triple=alpha-unknown-linux-gnu --show-encoding %s | FileCheck %s

# Operand forms and mnemonics the kernel's hand-written assembly uses, beyond
# what the code generator emits.

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
