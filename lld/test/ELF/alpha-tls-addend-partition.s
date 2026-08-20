# REQUIRES: alpha
## A GOT-based TLS relocation is keyed on its addend, so the demand a file puts
## on the GOT has to be counted the same way.  Counting one entry per symbol
## instead places this file in a partition that cannot hold it, and the entries
## past the end of that partition are out of the gp window's reach.
# RUN: llvm-mc -filetype=obj -triple=alpha-unknown-linux-gnu %S/Inputs/alpha-tls-addend-partition.s -o %t1.o
# RUN: llvm-mc -filetype=obj -triple=alpha-unknown-linux-gnu %s -o %t2.o
# RUN: ld.lld -T %S/Inputs/alpha-gp.script %t1.o %t2.o -o %t
# RUN: llvm-readelf -SW %t | FileCheck --check-prefix=SEC %s
# RUN: llvm-objdump -d --no-show-raw-insn %t | FileCheck %s

## 5120 literal entries and 4096 gottprel entries against the one symbol: over
## a partition together, so the second file gets a partition of its own.
# SEC: .got PROGBITS 0000000120010000 020000 012000

## The first partition's gp is .got + 0x8000.
# CHECK:      ldah $29, 2($27)
# CHECK-NEXT: lda $29, -32768($29)

## The second file's gp is .got + 5120 * 8 + 0x8000 = 0x120022000, so its first
## entry sits at the bottom of that window and its last, 4095 entries further
## on, is still inside it.  Had the file been counted as needing one entry it
## would have been given what was left of the first partition instead, and
## everything past that would have been out of reach.
# CHECK:      ldah $29, 2($27)
# CHECK-NEXT: lda $29, -12300($29)
# CHECK-NEXT: ldq $1, -32768($29)
# CHECK:      ldq $1, -8($29)

.text
.globl _start
.usepv _start, std
_start:
	ldgp $29, 0($27)
## 4096 distinct addends against x, one GOT entry each.
.irpc a, 01234567
.irpc b, 01234567
.irpc c, 01234567
.irpc d, 01234567
	ldq $1, x+\a\b\c\d($29)	!gottprel
.endr
.endr
.endr
.endr
	ret

.section .tdata,"awT",@progbits
.globl x
x:
	.zero 4096
