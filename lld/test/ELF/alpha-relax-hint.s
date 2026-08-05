# REQUIRES: alpha
## Relaxing a call whose jsr also carries an R_ALPHA_HINT.
##
## Every other relax test builds its LITUSE by hand with .reloc and so never
## produces a HINT at all, which left the whole relocation layout the backend
## actually emits -- LITERAL, then LITUSE and HINT sharing the jsr's offset --
## outside the lld suite.  This links that layout end to end.
##
## Note what this does not cover: the scan skips a HINT when deciding whether an
## intervening relocation ends the LITERAL/LITUSE group, and that arm is
## unreachable from real input.  Both GNU as and the backend write the LITUSE
## before the HINT, so the call is already recorded by the time the hint is
## seen; removing the skip does not change this test's result.
# RUN: llvm-mc -filetype=obj -triple=alpha-unknown-linux-gnu %s -o %t.o
# RUN: llvm-readelf -r %t.o | FileCheck --check-prefix=RELOC %s
# RUN: ld.lld -T %S/Inputs/alpha-gp.script %t.o -o %t
# RUN: llvm-objdump -d --no-show-raw-insn %t | FileCheck %s

## The input really does carry all three, with LITUSE and HINT at one offset.
# RELOC:      R_ALPHA_LITERAL {{.*}} callee
# RELOC-NEXT: R_ALPHA_LITUSE
# RELOC-NEXT: R_ALPHA_HINT {{.*}} callee

## The call becomes a direct branch.  This is what the HINT has to not prevent:
## an R_ALPHA_HINT sits at the same offset as the LITUSE, and if it were treated
## as ending the LITERAL/LITUSE group the literal would no longer be recorded as
## consumed by a call and the pair would never be relaxed at all.
# CHECK:      <_start>:
# CHECK:      bsr $26, 0x120000010 <callee>
# CHECK:      <callee>:

	.text
	.globl _start
_start:
	ldq $27, callee($29)	!literal!1
	jsr $26, ($27), callee	!lituse_jsr!1
	ldgp $29, 0($26)

	.globl callee
	.type callee,@function
	.hidden callee
callee:
	ret
