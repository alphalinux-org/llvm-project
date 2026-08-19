# RUN: llvm-mc -triple=alpha-unknown-linux-gnu -filetype=obj %s \
# RUN:   | llvm-objdump -d - | FileCheck %s

# The floating-point branches test $Fa against zero.  The code generator does
# not select them, so they exist for hand-written assembly; GMP's mpn loops
# count down in the float unit and branch on the result.

tgt:
# CHECK: ff ff 1f c4  	fbeq $f0, -1
	fbeq	$f0, tgt
# CHECK: fe ff 3f c8  	fblt $f1, -2
	fblt	$f1, tgt
# CHECK: fd ff 5f cc  	fble $f2, -3
	fble	$f2, tgt
# CHECK: fc ff 7f d4  	fbne $f3, -4
	fbne	$f3, tgt
# CHECK: fb ff 9f d8  	fbge $f4, -5
	fbge	$f4, tgt
# CHECK: fa ff bf dc  	fbgt $f5, -6
	fbgt	$f5, tgt
