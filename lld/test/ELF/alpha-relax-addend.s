# REQUIRES: alpha
## A GOT literal may name somewhere other than a symbol's entry point: the
## assembler turns a reference to a local target into a section symbol plus an
## addend, and hand-written assembly can write sym+N directly.  st_other
## describes the entry point and nothing else, so a call through such a literal
## must not be entered eight bytes in on the strength of it -- there is no gp
## load there to skip.
# RUN: llvm-mc -filetype=obj -triple=alpha-unknown-linux-gnu %s -o %t.o
# RUN: ld.lld -T %S/Inputs/alpha-gp.script %t.o -o %t
# RUN: llvm-objdump -d --no-show-raw-insn %t | FileCheck %s

	.text
	.globl _start
_start:
## The entry point itself: the callee advertises the standard gp load, so the
## load goes and the branch enters past it.
# CHECK:      120000000: ldq_u $31, 0($30)
## gpload + 8 - (0x120000004 + 4) = 0x14, >> 2 = 5.
# CHECK-NEXT: 120000004: bsr $26, 0x12000001c <gpload+0x8>
	ldq $27, gpload($29)	!literal
.Lentry:
	jsr $26, ($27)
	.reloc .Lentry, R_ALPHA_LITUSE, 3

## Eight bytes in, which is already past the gp load.  The branch must land
## exactly there, not another eight bytes further on.
# CHECK-NEXT: 120000008: ldq $27, -32768($29)
## gpload + 8 - (0x12000000c + 4) = 0xc, >> 2 = 3.
# CHECK-NEXT: 12000000c: bsr $26, 0x12000001c <gpload+0x8>
	ldq $27, gpload+8($29)	!literal
.Linside:
	jsr $26, ($27)
	.reloc .Linside, R_ALPHA_LITUSE, 3

	ret

	.globl gpload
	.usepv gpload, std
gpload:
	ldgp $29, 0($27)
	ret
