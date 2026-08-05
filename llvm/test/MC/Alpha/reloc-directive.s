# RUN: llvm-mc -filetype=obj -triple=alpha-unknown-linux-gnu %s -o %t
# RUN: llvm-readobj -r %t | FileCheck %s

## .reloc accepts the R_ALPHA_* names, which is the only way to request a
## relocation that no instruction operand produces on its own, such as the
## R_ALPHA_LITUSE annotations a linker needs in order to relax a call.

# CHECK:      Section ({{.*}}) .rela.text {
# CHECK-NEXT:   0x0 R_ALPHA_LITERAL callee 0x0
# CHECK-NEXT:   0x4 R_ALPHA_LITUSE - 0x3
# CHECK-NEXT:   0x8 R_ALPHA_REFQUAD callee 0x8
# CHECK-NEXT: }

	.text
	ldq $27, callee($29)	!literal
.Ljsr:
	jsr $26, ($27)
	.reloc .Ljsr, R_ALPHA_LITUSE, 3
	.reloc .Ljsr+4, R_ALPHA_REFQUAD, callee+8
	.quad 0

	.globl callee
callee:
	ret
