# RUN: llvm-mc -triple=alpha-unknown-linux-gnu --show-encoding %s | FileCheck %s

## ldgp expands to the ldah/lda pair that rebuilds the global pointer from the
## procedure value, tied together by a !gpdisp relocation with addend 4.  It
## names its own destination -- it is not always $29 -- and its displacement
## rides in the lda half.  Each encoding below is what GNU as 2.46.1 produces
## for the same line.

# CHECK: ldah $29, 4($27) !gpdisp
# CHECK: lda $29, 0($29)
	ldgp $29, 0($27)

# CHECK: ldah $0, 4($27) !gpdisp
# CHECK: lda $0, 0($0)
	ldgp $0, 0($27)

# CHECK: ldah $29, 4($27) !gpdisp
# CHECK: lda $29, 8($29)
# CHECK-SAME: encoding: [0x08,0x00,0xbd,0x23]
	ldgp $29, 8($27)
