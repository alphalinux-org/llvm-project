# RUN: llvm-mc -triple=alpha-unknown-linux-gnu -filetype=obj %s -o %t.o
# RUN: llvm-objdump -d %t.o | FileCheck %s
# RUN: not llvm-mc -triple=alpha-unknown-linux-gnu -filetype=obj %s \
# RUN:   --defsym ERR=1 -o /dev/null 2>&1 | FileCheck %s --check-prefix=ERR

## An expression with no relocation specifier -- the difference of two labels
## in the same section, as libffi's src/alpha/osf.S uses to index a jump table
## -- is folded by the assembler into the displacement rather than dropped.

	.text
	.globl f
f:
0:
	nop
	nop
# CHECK: lda $1, 24($26)
	lda	$1, 99f-0b($26)
# CHECK-NEXT: ldq $2, 24($26)
	ldq	$2, 99f-0b($26)
# CHECK-NEXT: addq $3, 24, $3
	addq	$3, 99f-0b, $3
# CHECK-NEXT: lda $4, 12($26)
	lda	$4, 8+4($26)
99:
	ret

.ifdef ERR
## Nothing can relocate a bare displacement, so a difference the assembler
## cannot fold has to be diagnosed rather than silently encoded as zero.
# ERR: error: expression is not an assembly-time constant
	lda	$5, undefined_sym-f($26)
.endif
