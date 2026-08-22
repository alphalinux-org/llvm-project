# RUN: llvm-mc -triple=alpha-unknown-linux-gnu -filetype=obj %s \
# RUN:   | llvm-objdump -d - | FileCheck %s

# The floating-point branches test $Fa against zero.  The code generator does
# not select them, so they exist for hand-written assembly; GMP's mpn loops
# count down in the float unit and branch on the result.  Every displacement
# here reaches back to tgt, which the disassembler resolves and names.

tgt:
# CHECK: ff ff 1f c4  	fbeq $f0, 0x0 <tgt>
	fbeq	$f0, tgt
# CHECK: fe ff 3f c8  	fblt $f1, 0x0 <tgt>
	fblt	$f1, tgt
# CHECK: fd ff 5f cc  	fble $f2, 0x0 <tgt>
	fble	$f2, tgt
# CHECK: fc ff 7f d4  	fbne $f3, 0x0 <tgt>
	fbne	$f3, tgt
# CHECK: fb ff 9f d8  	fbge $f4, 0x0 <tgt>
	fbge	$f4, tgt
# CHECK: fa ff bf dc  	fbgt $f5, 0x0 <tgt>
	fbgt	$f5, tgt
