# REQUIRES: alpha
## A general- or local-dynamic TLS sequence calls __tls_get_addr through its own
## GOT literal, so --relax rewrites all five instructions at once: the thread
## pointer is read with call_pal rduniq and the offset added to it directly.
## Local exec needs the offset to be a link-time constant; otherwise the offset
## still comes from the GOT, but through one entry instead of two plus the
## literal.
# RUN: llvm-mc -filetype=obj -triple=alpha-unknown-linux-gnu %s -o %t.o
# RUN: ld.lld -T %S/Inputs/alpha-gp.script %t.o -o %t
# RUN: llvm-objdump -d --no-show-raw-insn %t | FileCheck %s
# RUN: ld.lld -T %S/Inputs/alpha-gp.script --no-relax %t.o -o %t.norelax
# RUN: llvm-objdump -d --no-show-raw-insn %t.norelax | FileCheck --check-prefix=NORELAX %s

	.text
	.globl _start
_start:
## General dynamic against a local symbol: x is at offset 0 of a TLS block that
## starts 16 bytes past the thread pointer, so its offset is 16.
# CHECK:      ldah $16, 0($31)
# CHECK-NEXT: lda $16, 16($16)
# CHECK-NEXT: call_pal 158
# CHECK-NEXT: addq $16, $0, $0
# CHECK-NEXT: ldq_u $31, 0($30)
	lda $16, x($29)			!tlsgd
	ldq $27, __tls_get_addr($29)	!literal
.Lgd:
	jsr $26, ($27)
	.reloc .Lgd, R_ALPHA_LITUSE, 4
	ldgp $29, 0($26)

## Local dynamic resolves to the TLS block itself, so the offset is the block's
## own: 16, with the symbol's own offset added by a later dtprel relocation.
# CHECK-NEXT: ldah $16, 0($31)
# CHECK-NEXT: lda $16, 16($16)
# CHECK-NEXT: call_pal 158
# CHECK-NEXT: addq $16, $0, $0
# CHECK-NEXT: ldq_u $31, 0($30)
	lda $16, x($29)			!tlsldm
	ldq $27, __tls_get_addr($29)	!literal
.Lld:
	jsr $26, ($27)
	.reloc .Lld, R_ALPHA_LITUSE, 5
	ldgp $29, 0($26)

## With something in between, the offset cannot be split across the pair, so the
## sequence falls back to a single GOT load of the thread-pointer offset.
# CHECK-NEXT: ldq $16, -32768($29)
# CHECK-NEXT: nop
# CHECK-NEXT: ldq_u $31, 0($30)
# CHECK-NEXT: call_pal 158
# CHECK-NEXT: addq $16, $0, $0
# CHECK-NEXT: ldq_u $31, 0($30)
	lda $16, x($29)			!tlsgd
	nop
	ldq $27, __tls_get_addr($29)	!literal
.Lgd2:
	jsr $26, ($27)
	.reloc .Lgd2, R_ALPHA_LITUSE, 4
	ldgp $29, 0($26)
	ret

	.globl __tls_get_addr
	.type __tls_get_addr,@function
__tls_get_addr:
	ret

	.section .tdata,"awT",@progbits
	.globl x
x:	.long 42

## Without --relax the call stays, and so do both of its GOT entries.
# NORELAX:      lda $16, -32768($29)
# NORELAX-NEXT: ldq $27, -32752($29)
# NORELAX-NEXT: jsr $26, ($27)
