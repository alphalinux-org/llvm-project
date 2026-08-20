# REQUIRES: alpha
## A partition can be moved to make room for a file, but nothing can be done for
## a file that needs more GOT than one gp reaches. 8193 entries is one too many.
# RUN: llvm-mc -filetype=obj -triple=alpha-unknown-linux-gnu %s -o %t.o
# RUN: not ld.lld -T %S/Inputs/alpha-gp.script %t.o -o /dev/null 2>&1 | FileCheck %s

# CHECK: error: input file {{.*}}.o needs more than 64KB of GOT; split it into smaller objects

.text
.globl _start
.usepv _start, std
_start:
	ldgp $29, 0($27)
	ldq $1, one($29)	!literal
.irpc a, 01
.irpc b, 01234567
.irpc c, 01234567
.irpc d, 01234567
.irpc e, 01234567
	ldq $1, s\a\b\c\d\e($29)	!literal
.endr
.endr
.endr
.endr
.endr
	ret
.data
.globl one
one:	.quad 0
.irpc a, 01
.irpc b, 01234567
.irpc c, 01234567
.irpc d, 01234567
.irpc e, 01234567
.globl s\a\b\c\d\e
s\a\b\c\d\e:	.quad 0
.endr
.endr
.endr
.endr
.endr
