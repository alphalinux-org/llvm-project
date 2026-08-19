# RUN: llvm-mc -triple=alpha-unknown-linux-gnu -filetype=obj %s -o %t.o
# RUN: llvm-objdump -s -j .text %t.o | FileCheck %s

# The ECOFF/OSF procedure-descriptor directives are accepted and ignored; .end
# in particular must not stop assembly the way the generic .end would.  .word
# is a 16-bit datum on Alpha rather than the 32-bit one the generic parser
# assembles.

	.ent foo
foo:
	.frame $30, 16, $26
	.prologue 1
	.mask 0x4000000, -16
	.fmask 0x0, 0
	ret ($26)
	.end foo

	.word 0x1234, 4

# The ret is the only thing any of the directives around it contributed, and
# the two halfwords follow it -- so assembly carried on past .end rather than
# stopping there, which is what the generic directive would have done.
# CHECK: 0000 0180fa6b 34120400
