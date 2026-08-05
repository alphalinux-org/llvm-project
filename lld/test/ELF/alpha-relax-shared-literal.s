# REQUIRES: alpha
## Relaxation deletes GOT loads, so it needs to know every use of one. The only
## record is that GNU as pairs each R_ALPHA_LITUSE with the R_ALPHA_LITERAL it
## follows, so a lituse that follows something else means a load is being shared
## between sequences and nothing here can be trusted. Give up on the section
## rather than delete a load that is still live; bfd does not check, and
## miscompiles such objects.
# RUN: llvm-mc -filetype=obj -triple=alpha-unknown-linux-gnu %s -o %t.o
# RUN: ld.lld --relax -T %S/Inputs/alpha-gp.script %t.o -o %t
# RUN: llvm-objdump -d --no-show-raw-insn %t | FileCheck %s

	.text
	.globl _start
_start:
## Neither call is relaxed, not even the first, whose own relocations are well
## formed, and the load they share stays.
# CHECK:      ldq $27, -32768($29)
# CHECK-NEXT: jsr $26, ($27)
	ldq $27, callee($29)	!literal
.Lfirst:
	jsr $26, ($27)
	.reloc .Lfirst, R_ALPHA_LITUSE, 3

## A relocation that is neither a literal nor a hint ends the group, so the
## second call's lituse follows no literal of its own.
# CHECK-NEXT: ldah $1, 0($29)
# CHECK-NEXT: jsr $26, ($27)
	ldah $1, datum($29)	!gprelhigh
.Lsecond:
	jsr $26, ($27)
	.reloc .Lsecond, R_ALPHA_LITUSE, 3

	ret

	.globl callee
	.usepv callee, std
callee:
	ldgp $29, 0($27)
	ret

	.data
	.globl datum
datum:
	.quad 0
