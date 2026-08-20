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
	.eflag 48
	ret ($26)
	.end foo

	.word 0x1234, 4
	.align 3
	.byte 0x99
	.align 3
	.byte 0xaa

# .word emits two little-endian halfwords after the 4-byte ret, reaching offset
# 8; the first .align 3 is already at an 8-byte boundary and so pads nothing,
# which is why a second one follows the byte at offset 8: from offset 9 it has
# real work to do and pads out to 16.  Checking only the first would say nothing
# about whether .align pads at all.
# What matters here is that the second .align pads at all, reaching offset 16;
# which bytes fill the gap is the padding commit's business, not this one's.
# CHECK:      0000 0180fa6b 34120400 99
# CHECK-NEXT: 0010 aa
