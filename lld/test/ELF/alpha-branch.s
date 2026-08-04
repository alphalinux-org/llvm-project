# REQUIRES: alpha
## Test R_ALPHA_BRADDR and R_ALPHA_BRSGP. Both are 21-bit displacements in
## instruction units measured from the instruction after the branch. BRSGP
## additionally skips the callee's two-instruction gp load when the callee
## advertises STO_ALPHA_STD_GPLOAD in st_other.
# RUN: llvm-mc -filetype=obj -triple=alpha-unknown-linux-gnu %s -o %t.o
# RUN: ld.lld -T %S/Inputs/alpha-gp.script %t.o -o %t
# RUN: llvm-objdump -d --no-show-raw-insn %t | FileCheck %s

## target - (0x120000000 + 4) = 0x10, >> 2 = 4.
# CHECK:      120000000: br $31, 4
## target - (0x120000004 + 4) = 0xc, >> 2 = 3.
# CHECK-NEXT: 120000004: bsr $26, 3
## gpload - (0x120000008 + 4) = 0xc, plus 8 for the skipped gp load, >> 2 = 5.
# CHECK-NEXT: 120000008: bsr $26, 5
## nopv has no gp load to skip: 0xc >> 2 = 3.
# CHECK-NEXT: 12000000c: bsr $26, 3

.text
.globl _start
_start:
  br $31, target
  bsr $26, target
  bsr $26, gpload       !samegp
  bsr $26, nopv         !samegp
  ret
target:
  ret
gpload:
  .usepv gpload, std
  ret
nopv:
  .usepv nopv, no
  ret
