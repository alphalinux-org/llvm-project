# RUN: llvm-mc -triple=alpha-unknown-linux-gnu -filetype=obj %s -o %t.o
# RUN: llvm-objdump -s -j .text %t.o | FileCheck %s

# .word is a 16-bit datum on Alpha rather than the 32-bit one the generic
# parser assembles, which is what gcc's assembly output relies on.

	.word 0x1234, 4

# Two little-endian halfwords, four bytes in all rather than eight.
# CHECK: 0000 34120400
