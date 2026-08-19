# RUN: llvm-mc -triple=alpha-unknown-linux-gnu --show-encoding %s | FileCheck %s

# CHECK: addq $16, $17, $0        # encoding: [0x00,0x04,0x11,0x42]
	addq $16, $17, $0

# CHECK: sll $16, 3, $0           # encoding: [0x20,0x77,0x00,0x4a]
	sll $16, 3, $0

# CHECK: and $16, $17, $0         # encoding: [0x00,0x00,0x11,0x46]
	and $16, $17, $0

# CHECK: ldq $0, 8($16)           # encoding: [0x08,0x00,0x10,0xa4]
	ldq $0, 8($16)

# CHECK: stq $17, 16($30)         # encoding: [0x10,0x00,0x3e,0xb6]
	stq $17, 16($30)

# CHECK: addt $f16, $f0, $f0      # encoding: [0x00,0x14,0x00,0x5a]
	addt $f16, $f0, $f0

# CHECK: lda $0, 42($31)          # encoding: [0x2a,0x00,0x1f,0x20]
	lda $0, 42($31)
