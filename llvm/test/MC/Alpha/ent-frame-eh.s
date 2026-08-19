# RUN: llvm-mc -filetype=obj -triple=alpha-unknown-linux-gnu %s -o %t
# RUN: llvm-dwarfdump --debug-frame %t | FileCheck %s

## GNU as synthesises .eh_frame from the procedure directives when the file
## describes no frames itself (alpha_elf_md_finish), which is how the
## hand-written assembly in glibc -- setjmp, __longjmp, the string routines,
## the dynamic linker's trampoline -- gets unwind information without writing
## a single .cfi_* directive.  Without it those functions cannot be unwound
## through at all.

# CHECK:      FDE cie={{.*}} pc=00000000...0000001c
# CHECK:      DW_CFA_advance_loc: 12
# CHECK-NEXT: DW_CFA_def_cfa_offset: +32
# CHECK-NEXT: DW_CFA_offset: R26 -32
# CHECK-NEXT: DW_CFA_offset: R9 -24

	.text
	.align 4
	.globl fn
	.ent fn
fn:
	lda $30, -32($30)
	.frame $30, 32, $26, 0
	stq $26, 0($30)
	stq $9, 8($30)
	.mask 0x4000200, -32
	.fmask 0x0, 0
	.prologue 1
	ldq $9, 8($30)
	ldq $26, 0($30)
	lda $30, 32($30)
	ret $31, ($26), 1
	.end fn

## A frame register other than $30, and saved floating-point registers, which
## are DWARF numbers 32 and up.
# CHECK:      FDE cie={{.*}} pc=0000001c...00000024
# CHECK:      DW_CFA_advance_loc: 4
# CHECK-NEXT: DW_CFA_def_cfa: R15 +64
# CHECK-NEXT: DW_CFA_offset: F2 -16
	.globl fp
	.ent fp
fp:
	nop
	.frame $15, 64, $26, 0
	.fmask 0x4, -16
	.prologue 1
	ret $31, ($26), 1
	.end fp

## A procedure with no .prologue says nothing about where its frame is set up,
## so it gets no FDE -- glibc's ENTRY macro is written this way, and GNU as
## gives those no FDE either.
# CHECK-NOT: pc=00000024...
	.globl noprol
	.ent noprol, 0
noprol:
	ret $31, ($26), 1
	.end noprol
