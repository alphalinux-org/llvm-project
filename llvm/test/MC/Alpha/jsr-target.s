# RUN: llvm-mc -filetype=obj -triple=alpha-unknown-linux-gnu %s -o %t
# RUN: llvm-objdump -dr %t | FileCheck %s

## `jsr $Ra, ($Rb), target` names the call's target, which does not change the
## encoding: it fills the 14-bit hint field through an R_ALPHA_HINT, and a
## !lituse_jsr suffix additionally marks the call as the use of the GOT literal
## loaded just before it, which is what lets a linker relax the pair into a
## direct branch. GNU as writes the lituse first and bfd only looks at the
## relocation immediately after a literal's, so the order is not cosmetic.

# CHECK:      ldq $27, 0($29)
# CHECK-NEXT: R_ALPHA_LITERAL callee
# CHECK-NEXT: jsr $26, ($27)
# CHECK-NEXT: R_ALPHA_LITUSE *ABS*+0x3
# CHECK-NEXT: R_ALPHA_HINT callee
	ldq $27, callee($29)	!literal!1
	jsr $26, ($27), callee	!lituse_jsr!1

## Without the suffix, only the hint.
# CHECK-NEXT: jsr $26, ($27)
# CHECK-NEXT: R_ALPHA_HINT callee
	jsr $26, ($27), callee

## A tail call spells the same thing with a jmp.
# CHECK-NEXT: jmp $31, ($27), 0
# CHECK-NEXT: R_ALPHA_LITUSE *ABS*+0x3
# CHECK-NEXT: R_ALPHA_HINT callee
# CHECK-NEXT: jmp $31, ($27), 0
# CHECK-NEXT: R_ALPHA_HINT callee
	jmp $31, ($27), callee	!lituse_jsr!1
	jmp $31, ($27), callee

## The hint counts longwords from the instruction after the jump, but is
## spelled as a byte offset -- so a written 4 is a hint field of 1, and the
## disassembler spells it back the same way.  A value that is not a multiple of
## four cannot be encoded as written; it is assembled with the low bits dropped
## and a warning, as GNU as does (see paren-reg-forms.s).
# CHECK-NEXT: jsr $26, ($27), 4
# CHECK-NEXT: jmp $31, ($27), 8
	jsr $26, ($27), 4
	jmp $31, ($27), 8

## A plain number is a hint value, not a symbol, and needs no relocation.
# CHECK-NEXT: jsr $26, ($27)
# CHECK-NEXT: jmp $31, ($27), 0
# CHECK-NEXT: jsr $26, ($27)
# CHECK-NEXT: ret
	jsr $26, ($27), 0
	jmp $31, ($27), 0
	jsr $26, ($27)
	ret

	.globl callee
	.type callee,@function
callee:
	ret
