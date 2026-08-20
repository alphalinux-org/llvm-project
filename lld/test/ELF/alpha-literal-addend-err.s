# REQUIRES: alpha
## A GOT entry the dynamic linker fills in holds a symbol's address and nothing
## else, so R_ALPHA_LITERAL against a preemptible symbol cannot carry an addend.
## Diagnose it, and say which relocation it was: the error is reported while
## scanning, where the section and offset are still known.
# RUN: llvm-mc -filetype=obj -triple=alpha-unknown-linux-gnu %s -o %t.o
# RUN: not ld.lld -shared -T %S/Inputs/alpha-shared.script %t.o -o /dev/null 2>&1 | FileCheck %s

# CHECK: {{.*}}alpha-literal-addend-err.s.tmp.o:(.text+0x4): R_ALPHA_LITERAL against preemptible symbol 'preemptible' with a non-zero addend

	.text
	.globl _start
_start:
	ldq $27, preemptible($29)		!literal
	ldq $27, preemptible+8($29)		!literal
	ret

	.globl preemptible
	.type preemptible,@object
	.data
preemptible:
	.quad 0
	.quad 0
