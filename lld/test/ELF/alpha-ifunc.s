# REQUIRES: alpha
## Alpha has no PLT, so an ifunc is resolved straight into the GOT entry the
## caller loads from: R_ALPHA_IRELATIVE names the resolver in its addend and
## the runtime stores the result there. A data reference is resolved the same
## way, in place. This matches what bfd emits.
# RUN: llvm-mc -filetype=obj -triple=alpha-unknown-linux-gnu %s -o %t.o
# RUN: ld.lld -T %S/Inputs/alpha-gp.script %t.o -o %t
# RUN: llvm-readobj -r %t | FileCheck %s
# RUN: llvm-readelf -SW %t | FileCheck --check-prefix=SEC %s

## The relocation is not against a symbol; the addend is resolver's address.
## .got+0 is the entry the literal load uses and 0x120010008 is `ptr`.
# CHECK:      Section ({{.*}}) .rela.dyn {
# CHECK-NEXT:   0x120010000 R_ALPHA_IRELATIVE - 0x120000044
# CHECK-NEXT:   0x120010008 R_ALPHA_IRELATIVE - 0x120000044
# CHECK-NEXT: }

## No PLT is involved.
# SEC-NOT: .plt

	.text
	.globl _start
_start:
	ldgp $29, 0($27)
	ldq $27, fn($29)	!literal
	jsr $26, ($27)
	ret

	.globl resolver
	.type resolver,@function
resolver:
	ret

	.globl fn
	.type fn,@gnu_indirect_function
fn = resolver

	.data
	.globl ptr
ptr:	.quad fn
