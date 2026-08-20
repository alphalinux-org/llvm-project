# REQUIRES: alpha
## A partition is closed at a file boundary once what is left of it cannot hold
## the next file, however much of it that takes. Here the first input needs 3584
## entries and this one needs 5120: neither fills a partition on its own, but
## together they overrun one, so the second gets a partition of its own rather
## than being reported as needing more than 64KB of GOT.
# RUN: llvm-mc -filetype=obj -triple=alpha-unknown-linux-gnu %S/Inputs/alpha-got-partition.s -o %t1.o
# RUN: llvm-mc -filetype=obj -triple=alpha-unknown-linux-gnu %s -o %t2.o
# RUN: ld.lld -T %S/Inputs/alpha-gp.script %t1.o %t2.o -o %t
# RUN: llvm-readelf -SW %t | FileCheck --check-prefix=SEC %s
# RUN: llvm-objdump -d --no-show-raw-insn %t | FileCheck %s

## 3584 + 5120 entries, laid out back to back.
# SEC: .got PROGBITS 0000000120010000 020000 011000

## The first partition's gp is .got + 0x8000 = 0x120018000, reached from the
## ldah at 0x120000000.
# CHECK:      120000000: ldah $29, 2($27)
# CHECK-NEXT: 120000004: lda $29, -32768($29)

## The second starts at .got + 3584 * 8 = .got + 0x7000, so its gp is
## 0x12001f000 and its first entry sits at the bottom of it.
# CHECK:      12000380c: ldah $29, 2($27)
# CHECK-NEXT: 120003810: lda $29, -18444($29)
# CHECK-NEXT: 120003814: ldq $1, -32768($29)

.text
.globl _start
.usepv _start, std
_start:
	ldgp $29, 0($27)
.irpc a, 0123456789
.irpc b, 01234567
.irpc c, 01234567
.irpc d, 01234567
	ldq $1, q\a\b\c\d($29)	!literal
.endr
.endr
.endr
.endr
	ret
.data
.irpc a, 0123456789
.irpc b, 01234567
.irpc c, 01234567
.irpc d, 01234567
.globl q\a\b\c\d
q\a\b\c\d:	.quad 0
.endr
.endr
.endr
.endr
