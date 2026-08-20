# RUN: llvm-mc -triple=alpha-unknown-linux-gnu -filetype=obj %s -o %t.o
# RUN: llvm-readobj -r --symbols %t.o | FileCheck %s

## The TLS relocation-specifier suffixes GNU as accepts, including
## !gotdtprel, which local-dynamic uses to reach a module-relative offset
## through the GOT when it does not fit a 16-bit displacement.

# CHECK:      R_ALPHA_TLSGD x
# CHECK-NEXT: R_ALPHA_TLSLDM x
# CHECK-NEXT: R_ALPHA_DTPRELHI x
# CHECK-NEXT: R_ALPHA_DTPRELLO x
# CHECK-NEXT: R_ALPHA_GOTDTPREL x
# CHECK-NEXT: R_ALPHA_GOTTPREL x
# CHECK-NEXT: R_ALPHA_TPRELHI x
# CHECK-NEXT: R_ALPHA_TPRELLO x

## A symbol a TLS relocation refers to must be typed STT_TLS.
# CHECK:      Name: x
# CHECK:      Type: TLS

	.text
	lda $16, x($29)		!tlsgd
	lda $16, x($29)		!tlsldm
	ldah $1, x($16)		!dtprelhi
	lda $1, x($1)		!dtprello
	ldq $2, x($29)		!gotdtprel
	ldq $3, x($29)		!gottprel
	ldah $4, x($31)		!tprelhi
	lda $4, x($4)		!tprello
	ret

	.section .tdata,"awT",@progbits
	.globl x
x:	.quad 0
