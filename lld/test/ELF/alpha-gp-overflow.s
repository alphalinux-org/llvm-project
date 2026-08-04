# REQUIRES: alpha
## gp-relative displacements are only 16 bits wide. Diagnose data placed out of
## reach of gp, and a GOT that grows past the 64KB a single gp can address.
# RUN: llvm-mc -filetype=obj -triple=alpha-unknown-linux-gnu %s -o %t.o
# RUN: not ld.lld -T %S/Inputs/alpha-gp-overflow.script %t.o -o /dev/null 2>&1 \
# RUN:   | FileCheck %s

# CHECK: relocation R_ALPHA_GPREL16 out of range: 1081344 is not in [-32768, 32767]

.text
.globl _start
_start:
  lda $2, far($29)      !gprel
  ret

.data
.globl far
far:
  .quad 0
