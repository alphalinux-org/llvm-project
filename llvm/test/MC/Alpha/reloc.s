# RUN: llvm-mc -triple=alpha-unknown-linux-gnu -filetype=obj %s \
# RUN:   | llvm-readobj -r - | FileCheck %s
# RUN: llvm-mc -triple=alpha-unknown-linux-gnu --show-encoding %s \
# RUN:   | FileCheck %s --check-prefix=ASM

# CHECK: R_ALPHA_GPDISP
# CHECK: R_ALPHA_LITERAL g
# CHECK: R_ALPHA_GPRELHIGH g
# CHECK: R_ALPHA_GPRELLOW g
# CHECK: R_ALPHA_GPREL16 g

# The object path is not the only one these expressions reach: printing one
# needs a printSpecifierExpr of its own, and without it the base class's
# unreachable is hit instead of the suffix being written back out.
# ASM:      ldah $29, 4($27) !gpdisp
# ASM-NEXT: {{.*}}kind: fixup_alpha_gpdisp
# ASM:      lda $29, 0($29)
# ASM:      ldq $27, g($29) !literal
# ASM-NEXT: {{.*}}kind: fixup_alpha_literal
# ASM:      ldah $0, g($29) !gprelhigh
# ASM-NEXT: {{.*}}kind: fixup_alpha_gprelhigh
# ASM:      lda $0, g($0) !gprellow
# ASM-NEXT: {{.*}}kind: fixup_alpha_gprellow
# ASM:      lda $0, g($29) !gprel
# ASM-NEXT: {{.*}}kind: fixup_alpha_gprel16
	ldgp $29, 0($27)
	ldq $27, g($29)		!literal
	jsr $26, ($27)
	ldah $0, g($29)		!gprelhigh
	lda $0, g($0)		!gprellow
# The 16-bit gp-relative form, which GNU as spells !gprel.
	lda $0, g($29)		!gprel
	ret
