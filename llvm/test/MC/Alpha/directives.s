# RUN: llvm-mc -triple=alpha-unknown-linux-gnu -filetype=obj %s -o %t.o
# RUN: llvm-objdump -s -j .text %t.o | FileCheck %s

# The ECOFF/OSF procedure-descriptor directives are accepted and ignored; .end
# in particular must not stop assembly the way the generic .end would.  .word is
# a 16-bit datum on Alpha, and .align aligns to a power of two.

	.ent foo
foo:
	.frame $30, 16, $26
	.prologue 1
	.mask 0x4000000, -16
	.fmask 0x0, 0
	ret ($26)
	.end foo

	.word 0x1234, 4
	.align 3
	.byte 0x99

# .word emits two little-endian halfwords after the 4-byte ret; .align 3 pads to
# an 8-byte boundary (already aligned here) before the byte.
# CHECK: 0000 0180fa6b 34120400 99
