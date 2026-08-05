# REQUIRES: alpha
## A call the linker cannot resolve to a fixed address is never relaxed: a
## preemptible symbol is whatever the dynamic linker picks, and an ifunc is
## whatever its resolver returns. bfd learned the second one the hard way.
# RUN: llvm-mc -filetype=obj -triple=alpha-unknown-linux-gnu %s -o %t.o
# RUN: ld.lld -shared -T %S/Inputs/alpha-gp.script %t.o -o %t.so
# RUN: llvm-objdump -d --no-show-raw-insn %t.so | FileCheck --check-prefix=DSO %s
# RUN: ld.lld -T %S/Inputs/alpha-gp.script %t.o -o %t
# RUN: llvm-objdump -d --no-show-raw-insn %t | FileCheck --check-prefix=EXE %s

	.text
	.globl _start
_start:
## Preemptible in the shared library, resolved to a local definition in the
## executable.
# DSO:      ldq $27, -32768($29)
# DSO-NEXT: jsr $26, ($27)
# EXE:      ldq_u $31, 0($30)
# EXE-NEXT: bsr $26, 0x12000003c <preempt+0x8>
	ldq $27, preempt($29)	!literal
.Lpreempt:
	jsr $26, ($27)
	.reloc .Lpreempt, R_ALPHA_LITUSE, 3

## Hidden, so it cannot be preempted even in the shared library. Both callees
## advertise the standard gp load, so wherever the call is relaxed the load
## goes away with it.
# DSO-NEXT: ldq_u $31, 0($30)
# DSO-NEXT: bsr $26, 0x120000170 <hidden+0x8>
# EXE-NEXT: ldq_u $31, 0($30)
# EXE-NEXT: bsr $26, 0x120000048 <hidden+0x8>
	ldq $27, hidden($29)	!literal
.Lhidden:
	jsr $26, ($27)
	.reloc .Lhidden, R_ALPHA_LITUSE, 3

## An ifunc keeps its GOT load in both, since the address it will hold is only
## known once the resolver has run. In the executable it is the first entry of
## the table: the two the relaxed calls no longer read were given back. In the
## shared library nothing can be, because every entry there carries a dynamic
## relocation.
# DSO-NEXT: ldq $27, -32752($29)
# DSO-NEXT: jsr $26, ($27)
# EXE-NEXT: ldq $27, -32768($29)
# EXE-NEXT: jsr $26, ($27)
	ldq $27, ifn($29)	!literal
.Lifn:
	jsr $26, ($27)
	.reloc .Lifn, R_ALPHA_LITUSE, 3
	ret

	.globl preempt
	.usepv preempt, std
preempt:
	ldgp $29, 0($27)
	ret

	.globl hidden
	.hidden hidden
	.usepv hidden, std
hidden:
	ldgp $29, 0($27)
	ret

	.globl resolver
	.type resolver,@function
resolver:
	ret

	.globl ifn
	.type ifn,@gnu_indirect_function
ifn = resolver
