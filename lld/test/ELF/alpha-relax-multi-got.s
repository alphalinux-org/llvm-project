# REQUIRES: alpha
## A call whose callee belongs to another GOT partition can still be relaxed
## into a direct branch, but not into one that skips the callee's gp load: that
## shortcut is only sound when the callee would compute the gp the caller
## already has, and the whole point of a second partition is that it does not.
# RUN: llvm-mc -filetype=obj -triple=alpha-unknown-linux-gnu %S/Inputs/alpha-multi-got.s -o %t1.o
# RUN: llvm-mc -filetype=obj -triple=alpha-unknown-linux-gnu %s -o %t2.o
# RUN: ld.lld -T %S/Inputs/alpha-gp.script %t1.o %t2.o -o %t
# RUN: llvm-objdump -d --no-show-raw-insn %t | FileCheck %s

## The load stays, and the branch lands on f1 itself rather than eight bytes in.
# CHECK:      120008014: ldq $27, -32768($29)
## f1 - (0x120008018 + 4) = -0x801c, >> 2 = -8199.
# CHECK-NEXT: 120008018: bsr $26, 0x120000000 <f1>

.text
.globl _start
_start:
  ldgp $29, 0($27)
  ldq $27, f1($29)	!literal
.Lcall:
  jsr $26, ($27)
  .reloc .Lcall, R_ALPHA_LITUSE, 3
  ret
