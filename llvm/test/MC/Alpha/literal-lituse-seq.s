# RUN: llvm-mc -filetype=obj -triple=alpha-unknown-linux-gnu %s -o %t
# RUN: llvm-readobj -r %t | FileCheck %s

## bfd works out what a GOT literal is used for by walking the relocations that
## follow the LITERAL until it reaches one that is not a LITUSE
## (elf64_alpha_relax_section), so a LITUSE has to sit directly behind the
## literal it names.  GNU as guarantees that with the `!N' sequence numbers,
## reordering .rela.text to suit; written in address order instead, the LITUSE
## below lands behind g's literal and the linker relaxes the call to g.

# CHECK:      .rela.text {
# CHECK:        0x10 R_ALPHA_LITERAL .text 0x{{.*}}
# CHECK-NEXT:   0x18 R_ALPHA_LITUSE - 0x3
# CHECK-NEXT:   0x14 R_ALPHA_LITERAL .text 0x{{.*}}
## The marker relocations that carry the pairing are the assembler's own
## bookkeeping and never reach the file.
# CHECK-NOT:    R_ALPHA_NONE

	.text
	.globl main
	.ent main
main:
	ldgp $29, 0($27)
	lda $30, -16($30)
	stq $26, 0($30)
	ldq  $27, f($29)!literal!1
	ldq  $1,  g($29)!literal!2
	jsr  $26, ($27), f!lituse_jsr!1
	ldah $29, 0($26)!gpdisp!3
	lda  $29, 0($29)!gpdisp!3
	ldq $26, 0($30)
	lda $30, 16($30)
	ret $31, ($26), 1
	.end main

f:	lda $0, 111($31)
	ret $31, ($26), 1
g:	lda $0, 222($31)
	ret $31, ($26), 1

## A sequence number that names no literal is an error, as it is in GNU as:
## there is nothing for the use to be relaxed against.
# RUN: not llvm-mc -filetype=obj -triple=alpha-unknown-linux-gnu \
# RUN:   --defsym ORPHAN=1 %s -o /dev/null 2>&1 | FileCheck %s --check-prefix=ERR
# ERR: error: no !literal!9 was found
.ifdef ORPHAN
	jsr $26, ($27), f!lituse_jsr!9
.endif
