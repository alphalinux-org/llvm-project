# REQUIRES: alpha
## GOT entries are allocated while scanning, long before any address is known,
## so an entry that no surviving load reads is given back afterwards and the
## rest slide down over it. That moves .got, and everything laid out after it,
## which is why relaxation runs as a layout pass.
# RUN: llvm-mc -filetype=obj -triple=alpha-unknown-linux-gnu %s -o %t.o
# RUN: ld.lld -T %S/Inputs/alpha-gp.script %t.o -o %t
# RUN: llvm-objdump -d --no-show-raw-insn %t | FileCheck %s
# RUN: llvm-readelf -SW %t | FileCheck --check-prefix=SEC %s
# RUN: ld.lld -T %S/Inputs/alpha-gp.script --no-relax %t.o -o %t.norelax
# RUN: llvm-readelf -SW %t.norelax | FileCheck --check-prefix=SEC-NORELAX %s

## Three entries are allocated: one for each of the two relaxed calls and one
## for the load that stays. Only the last survives, and it moves to the front.
# SEC:         .got PROGBITS 0000000120010000 {{[0-9a-f]+}} 000008
# SEC-NORELAX: .got PROGBITS 0000000120010000 {{[0-9a-f]+}} 000018

	.text
	.globl _start
_start:
# CHECK:      ldq_u $31, 0($30)
# CHECK-NEXT: bsr $26,
	ldq $27, first($29)	!literal
.L1:
	jsr $26, ($27)
	.reloc .L1, R_ALPHA_LITUSE, 3

# CHECK-NEXT: ldq_u $31, 0($30)
# CHECK-NEXT: bsr $26,
	ldq $27, second($29)	!literal
.L2:
	jsr $26, ($27)
	.reloc .L2, R_ALPHA_LITUSE, 3

## Nothing marks this one as a call, so its entry is still read -- from the
## start of the table now that the other two are gone.
# CHECK-NEXT: ldq $27, -32768($29)
# CHECK-NEXT: jsr $26, ($27)
	ldq $27, kept($29)	!literal
	jsr $26, ($27)
	ret

	.globl first
	.usepv first, no
first:
	ret

	.globl second
	.usepv second, no
second:
	ret

	.globl kept
	.usepv kept, no
kept:
	ret
