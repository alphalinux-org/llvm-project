# REQUIRES: alpha
## Two things an ifunc cannot do on Alpha, both diagnosed rather than
## mislinked: be branched to directly, since without a PLT there is no stub
## and the ifunc has no link-time address; and be resolved into a read-only
## section, since the runtime has to write the resolved address there.
# RUN: llvm-mc -filetype=obj -triple=alpha-unknown-linux-gnu %s -o %t.o
# RUN: not ld.lld %t.o -o /dev/null 2>&1 | FileCheck %s

# CHECK-DAG: cannot branch directly to ifunc 'fn'; it must be called through the GOT
# CHECK-DAG: cannot resolve ifunc 'fn' in read-only section .rodata

	.text
	.globl _start
_start:
	bsr $26, fn
	ret

	.globl resolver
	.type resolver,@function
resolver:
	ret

	.globl fn
	.type fn,@gnu_indirect_function
fn = resolver

	.section .rodata,"a",@progbits
	.quad fn
