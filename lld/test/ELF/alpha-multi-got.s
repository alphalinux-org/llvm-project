# REQUIRES: alpha
## One gp only reaches 64KB of GOT, so input files are grouped into partitions
## that each get their own gp. The first input here uses a whole partition, so
## the second starts a new one and gets its own copy of the entry it shares with
## the first.
# RUN: llvm-mc -filetype=obj -triple=alpha-unknown-linux-gnu %S/Inputs/alpha-multi-got.s -o %t1.o
# RUN: llvm-mc -filetype=obj -triple=alpha-unknown-linux-gnu %s -o %t2.o
# RUN: ld.lld -T %S/Inputs/alpha-gp.script %t1.o %t2.o -o %t
# RUN: llvm-readelf -SW %t | FileCheck --check-prefix=SEC %s
# RUN: llvm-objdump -d --no-show-raw-insn %t | FileCheck %s

## 8192 entries for the first partition and one for the second.
# SEC: .got PROGBITS 0000000120010000 020000 010008

## .got is at 0x120010000, so the first partition's gp is 0x120018000:
## 2 * 0x10000 - 0x8000 from the ldah's own address.
# CHECK:      120000000: ldah $29, 2($27)
# CHECK-NEXT: 120000004: lda $29, -32768($29)

## The second partition starts at .got+0x10000, so its gp is 0x120028000, which
## is a different displacement from the ldah at 0x12000800c. s00000 is the first
## entry of that partition even though the first input already has an entry for
## it.
# CHECK:      12000800c: ldah $29, 2($27)
# CHECK-NEXT: 120008010: lda $29, -12($29)
# CHECK-NEXT: 120008014: ldq $1, -32768($29)

.text
.globl _start
_start:
  ldgp $29, 0($27)
  ldq $1, s00000($29)    !literal
  ret
