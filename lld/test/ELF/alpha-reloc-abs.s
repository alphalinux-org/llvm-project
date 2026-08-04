# REQUIRES: alpha
## Test R_ALPHA_REFLONG, R_ALPHA_REFQUAD and the PC-relative R_ALPHA_SREL*.
# RUN: llvm-mc -filetype=obj -triple=alpha-unknown-linux-gnu %s -o %t.o
# RUN: ld.lld %t.o --section-start=.rodata=0x120030000 \
# RUN:   --section-start=.data=0x120040000 -o %t
# RUN: llvm-objdump -s -j .data %t | FileCheck %s

# CHECK:      Contents of section .data:
## sym = 0x120030000; the low word is 0x20030000.
## sym - 0x120040008 = -0x10008.  The .quad after it is aligned to 8, as GNU as
## aligns a data directive, so it sits at 0x10: sym - 0x120040010 = -0x10010.
# CHECK-NEXT: 120040000 00000320 01000000 f8fffeff 00000000
# CHECK-NEXT: 120040010 f0fffeff ffffffff

.rodata
sym:
  .quad 0

.data
  .quad sym
  .long sym-.
  .quad sym-.
