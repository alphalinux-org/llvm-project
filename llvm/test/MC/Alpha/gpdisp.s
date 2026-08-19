# RUN: llvm-mc -triple=alpha-unknown-linux-gnu -filetype=obj %s \
# RUN:   | llvm-readobj -r - | FileCheck %s

# R_ALPHA_GPDISP addend = byte distance from ldah to lda.
# ldgp macro emits ldah+lda back-to-back: addend = 4.
# !gpdisp!N pair with an instruction between ldah and lda: addend = 8.

	.text
	# ldgp macro: ldah at +0, lda at +4
	ldgp $29, 0($27)

	# explicit !gpdisp!1 pair with call_pal between: ldah at +8, lda at +16
1:	ldah $1, 0($1)	!gpdisp!1
	call_pal 0x9e
	lda $1, 0($1)	!gpdisp!1

# CHECK:      R_ALPHA_GPDISP - 0x4
# CHECK-NEXT: R_ALPHA_GPDISP - 0x8

## A sequence number names one ldah/lda pair; a third use of it is a mistake,
## not the start of another pair.
# RUN: not llvm-mc -triple=alpha-unknown-linux-gnu -filetype=obj \
# RUN:   --defsym ERR=1 %s -o /dev/null 2>&1 | FileCheck %s --check-prefix=ERR
.ifdef ERR
	ldah $29, 0($27)	!gpdisp!9
	lda $29, 0($29)		!gpdisp!9
# ERR: error: !gpdisp!9 is already paired
	lda $29, 0($29)		!gpdisp!9
.endif
