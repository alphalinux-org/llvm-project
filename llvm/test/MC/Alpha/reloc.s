# RUN: llvm-mc -triple=alpha-unknown-linux-gnu -filetype=obj %s \
# RUN:   | llvm-readobj -r - | FileCheck %s

# The assembler understands the ldgp macro, the bare jsr word and the
# relocation-specifier suffixes.

# CHECK: R_ALPHA_GPDISP
# CHECK: R_ALPHA_LITERAL g
# CHECK: R_ALPHA_GPRELHIGH g
# CHECK: R_ALPHA_GPRELLOW g
	ldgp $29, 0($27)
	ldq $27, g($29)		!literal
	jsr $26, ($27)
	ldah $0, g($29)		!gprelhigh
	lda $0, g($0)		!gprellow
	ret
