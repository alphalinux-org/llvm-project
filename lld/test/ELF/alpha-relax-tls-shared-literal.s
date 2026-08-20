# REQUIRES: alpha
## Relaxation deletes GOT loads, so it needs to know every use of one. The only
## record is that GNU as pairs each R_ALPHA_LITUSE with the R_ALPHA_LITERAL it
## follows, so a lituse that follows no literal at all means a load is being
## shared between sequences and nothing here can be trusted. llvm's own code
## generator emits exactly this: it loads __tls_get_addr once and reuses it for
## every TLS sequence in the function. Give up on the section rather than delete
## a load that is still live; bfd does not check, and miscompiles such objects.
# RUN: llvm-mc -filetype=obj -triple=alpha-unknown-linux-gnu %s -o %t.o
# RUN: ld.lld -T %S/Inputs/alpha-gp.script %t.o -o %t
# RUN: llvm-objdump -d --no-show-raw-insn %t | FileCheck %s

## Neither sequence is touched, not even the first, whose own relocations are
## well formed.
# CHECK-NOT:  call_pal
# CHECK:      lda $16, -32768($29)
# CHECK-NEXT: ldq $27, -32752($29)
# CHECK-NEXT: jsr $26, ($27)
# CHECK:      lda $16, -32744($29)
# CHECK-NEXT: jsr $26, ($27)
# CHECK-NOT:  call_pal

	.text
	.globl _start
_start:
	ldgp $29, 0($27)
	lda $16, x($29)			!tlsgd
	ldq $27, __tls_get_addr($29)	!literal
.Lfirst:
	jsr $26, ($27)
	.reloc .Lfirst, R_ALPHA_LITUSE, 4
	ldgp $29, 0($26)

## The second sequence reuses the address loaded for the first, so it has a
## lituse of its own but no literal.
	lda $16, y($29)			!tlsgd
.Lsecond:
	jsr $26, ($27)
	.reloc .Lsecond, R_ALPHA_LITUSE, 4
	ldgp $29, 0($26)
	ret

	.globl __tls_get_addr
	.type __tls_get_addr,@function
__tls_get_addr:
	ret

	.section .tdata,"awT",@progbits
	.globl x
x:	.long 1
	.globl y
y:	.long 2
