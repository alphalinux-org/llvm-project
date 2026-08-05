# REQUIRES: alpha
## A callee whose st_other says nothing about its procedure value may still
## begin with the two-instruction gp load, which its R_ALPHA_GPDISP gives away.
## Hand-written assembly that never declared itself with .prologue or .usepv
## looks like this, and a call to it relaxes like one that did.
# RUN: llvm-mc -filetype=obj -triple=alpha-unknown-linux-gnu %s -o %t.o
# RUN: ld.lld -T %S/Inputs/alpha-gp.script %t.o -o %t
# RUN: llvm-objdump -d --no-show-raw-insn %t | FileCheck %s

	.text
	.globl _start
_start:
## A gp load at the start of the section, which is not any of the callees'.
# CHECK:      120000000: ldah $29,
# CHECK-NEXT: 120000004: lda $29,
	ldgp $29, 0($27)
## The load goes and the call enters past the gp load, exactly as it would if
## the callee were marked STO_ALPHA_STD_GPLOAD.
# CHECK-NEXT: 120000008: ldq_u $31, 0($30)
## implicit + 8 - (0x12000000c + 4) = 0x20, >> 2 = 8.
# CHECK-NEXT: 12000000c: bsr $26, 0x120000030 <implicit+0x8>
	ldq $27, implicit($29)	!literal
.Limplicit:
	jsr $26, ($27)
	.reloc .Limplicit, R_ALPHA_LITUSE, 3

## A callee that starts with something else keeps both its load and its entry
## point: nothing here says it does not need its own address. Its entry is the
## first of the table, the one the relaxed call gave back having been reclaimed.
# CHECK-NEXT: 120000010: ldq $27, -32768($29)
## plain - (0x120000014 + 4) = 0x1c, >> 2 = 7.
# CHECK-NEXT: 120000014: bsr $26, 0x120000034 <plain>
	ldq $27, plain($29)	!literal
.Lplain:
	jsr $26, ($27)
	.reloc .Lplain, R_ALPHA_LITUSE, 3
## A call to a local function goes through the section symbol plus an offset,
## so the gp load to look for is the one at that offset -- not the one at the
## start of the section, which is some other function's.
# CHECK-NEXT: 120000018: ldq $27, -32760($29)
## leaf - (0x12000001c + 4) = 0x4, >> 2 = 1.
# CHECK-NEXT: 12000001c: bsr $26, 0x120000024 <leaf>
	ldq $27, leaf($29)	!literal
.Lleaf:
	jsr $26, ($27)
	.reloc .Lleaf, R_ALPHA_LITUSE, 3
	ret

leaf:
	ret

	.globl implicit
implicit:
	ldgp $29, 0($27)
	ret

	.globl plain
plain:
	ret
