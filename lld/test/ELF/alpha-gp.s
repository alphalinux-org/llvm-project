# REQUIRES: alpha
## Test the gp-relative relocations: R_ALPHA_GPDISP patches an ldah/lda pair to
## materialize gp, and the rest are 16-bit displacements from it. gp is defined
## as .got + 0x8000.
# RUN: llvm-mc -filetype=obj -triple=alpha-unknown-linux-gnu %s -o %t.o
# RUN: ld.lld -T %S/Inputs/alpha-gp.script %t.o -o %t
# RUN: llvm-objdump -d --no-show-raw-insn %t | FileCheck %s

## .got is at 0x120010000, so gp is 0x120018000 and g is at 0x120010008.

## ldgp: gp - 0x120000000 = 0x18000. The lda sign-extends, so the ldah carries
## the borrow: 2 * 0x10000 - 0x8000 = 0x18000.
# CHECK:      120000000: ldah $29, 2($27)
# CHECK-NEXT: 120000004: lda $29, -32768($29)

## A gpdisp pair with an instruction in between (addend 8):
## gp - 0x120000008 = 0x17ff8 = 1 * 0x10000 + 0x7ff8.
# CHECK-NEXT: 120000008: ldah $1, 1($1)
# CHECK-NEXT: 12000000c: call_pal 158
# CHECK-NEXT: 120000010: lda $1, 32760($1)

## R_ALPHA_LITERAL: g's GOT entry is at .got+0, i.e. gp-0x8000.
# CHECK-NEXT: 120000014: ldq $27, -32768($29)

## R_ALPHA_GPRELHIGH/GPRELLOW: g - gp = -0x7ff8 = 0 * 0x10000 - 0x7ff8.
# CHECK-NEXT: 120000018: ldah $0, 0($29)
# CHECK-NEXT: 12000001c: lda $0, -32760($0)

## R_ALPHA_GPREL16.
# CHECK-NEXT: 120000020: lda $2, -32760($29)

.text
.globl _start
_start:
  ldgp $29, 0($27)
1:
  ldah $1, 0($1)        !gpdisp!1
  call_pal 0x9e
  lda $1, 0($1)         !gpdisp!1
  ldq $27, g($29)       !literal
  ldah $0, g($29)       !gprelhigh
  lda $0, g($0)         !gprellow
  lda $2, g($29)        !gprel
  ret

.data
.globl g
g:
  .quad 0
