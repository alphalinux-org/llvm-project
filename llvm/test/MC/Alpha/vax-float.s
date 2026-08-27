# RUN: llvm-mc -triple=alpha-unknown-linux-gnu -filetype=obj %s \
# RUN:   | llvm-objdump -d - | FileCheck %s
# RUN: not llvm-mc -triple=alpha-unknown-linux-gnu %s --defsym BAD=1 -o /dev/null 2>&1 \
# RUN:   | FileCheck %s --check-prefix=BAD

# The VAX F_floating and G_floating instructions (primary opcode 0x15) and the
# four VAX memory operations.  Nothing selects any of them -- C has no VAX
# floating type -- but they are part of the architecture every Alpha
# implements, so the assembler has to be able to write them and the
# disassembler to read them back.  Every encoding below was checked against GNU
# as for the same source line.
#
# The RUN line assembles and then disassembles, so each CHECK pins both
# directions at once: the bytes say what was emitted and the mnemonic says what
# those bytes decode to.

	.text

# F_floating arithmetic.  The trap field holds bare, /u, /s or /su -- there
# is no inexact bit in a VAX format -- and the rounding letter is /c or nothing.
	addf $f1, $f2, $f3
	addf/c $f1, $f2, $f3
	addf/u $f1, $f2, $f3
	addf/uc $f1, $f2, $f3
	addf/s $f1, $f2, $f3
	addf/sc $f1, $f2, $f3
	addf/su $f1, $f2, $f3
	addf/suc $f1, $f2, $f3
	subf $f1, $f2, $f3
	subf/c $f1, $f2, $f3
	subf/u $f1, $f2, $f3
	subf/uc $f1, $f2, $f3
	subf/s $f1, $f2, $f3
	subf/sc $f1, $f2, $f3
	subf/su $f1, $f2, $f3
	subf/suc $f1, $f2, $f3
	mulf $f1, $f2, $f3
	mulf/c $f1, $f2, $f3
	mulf/u $f1, $f2, $f3
	mulf/uc $f1, $f2, $f3
	mulf/s $f1, $f2, $f3
	mulf/sc $f1, $f2, $f3
	mulf/su $f1, $f2, $f3
	mulf/suc $f1, $f2, $f3
	divf $f1, $f2, $f3
	divf/c $f1, $f2, $f3
	divf/u $f1, $f2, $f3
	divf/uc $f1, $f2, $f3
	divf/s $f1, $f2, $f3
	divf/sc $f1, $f2, $f3
	divf/su $f1, $f2, $f3
	divf/suc $f1, $f2, $f3

# CHECK:      03 10 22 54 {{.*}}addf $f1, $f2, $f3
# CHECK-NEXT: 03 00 22 54 {{.*}}addf/c $f1, $f2, $f3
# CHECK-NEXT: 03 30 22 54 {{.*}}addf/u $f1, $f2, $f3
# CHECK-NEXT: 03 20 22 54 {{.*}}addf/uc $f1, $f2, $f3
# CHECK-NEXT: 03 90 22 54 {{.*}}addf/s $f1, $f2, $f3
# CHECK-NEXT: 03 80 22 54 {{.*}}addf/sc $f1, $f2, $f3
# CHECK-NEXT: 03 b0 22 54 {{.*}}addf/su $f1, $f2, $f3
# CHECK-NEXT: 03 a0 22 54 {{.*}}addf/suc $f1, $f2, $f3
# CHECK-NEXT: 23 10 22 54 {{.*}}subf $f1, $f2, $f3
# CHECK-NEXT: 23 00 22 54 {{.*}}subf/c $f1, $f2, $f3
# CHECK-NEXT: 23 30 22 54 {{.*}}subf/u $f1, $f2, $f3
# CHECK-NEXT: 23 20 22 54 {{.*}}subf/uc $f1, $f2, $f3
# CHECK-NEXT: 23 90 22 54 {{.*}}subf/s $f1, $f2, $f3
# CHECK-NEXT: 23 80 22 54 {{.*}}subf/sc $f1, $f2, $f3
# CHECK-NEXT: 23 b0 22 54 {{.*}}subf/su $f1, $f2, $f3
# CHECK-NEXT: 23 a0 22 54 {{.*}}subf/suc $f1, $f2, $f3
# CHECK-NEXT: 43 10 22 54 {{.*}}mulf $f1, $f2, $f3
# CHECK-NEXT: 43 00 22 54 {{.*}}mulf/c $f1, $f2, $f3
# CHECK-NEXT: 43 30 22 54 {{.*}}mulf/u $f1, $f2, $f3
# CHECK-NEXT: 43 20 22 54 {{.*}}mulf/uc $f1, $f2, $f3
# CHECK-NEXT: 43 90 22 54 {{.*}}mulf/s $f1, $f2, $f3
# CHECK-NEXT: 43 80 22 54 {{.*}}mulf/sc $f1, $f2, $f3
# CHECK-NEXT: 43 b0 22 54 {{.*}}mulf/su $f1, $f2, $f3
# CHECK-NEXT: 43 a0 22 54 {{.*}}mulf/suc $f1, $f2, $f3
# CHECK-NEXT: 63 10 22 54 {{.*}}divf $f1, $f2, $f3
# CHECK-NEXT: 63 00 22 54 {{.*}}divf/c $f1, $f2, $f3
# CHECK-NEXT: 63 30 22 54 {{.*}}divf/u $f1, $f2, $f3
# CHECK-NEXT: 63 20 22 54 {{.*}}divf/uc $f1, $f2, $f3
# CHECK-NEXT: 63 90 22 54 {{.*}}divf/s $f1, $f2, $f3
# CHECK-NEXT: 63 80 22 54 {{.*}}divf/sc $f1, $f2, $f3
# CHECK-NEXT: 63 b0 22 54 {{.*}}divf/su $f1, $f2, $f3
# CHECK-NEXT: 63 a0 22 54 {{.*}}divf/suc $f1, $f2, $f3

# G_floating arithmetic, the same set at the G function codes.
	addg $f1, $f2, $f3
	addg/c $f1, $f2, $f3
	addg/u $f1, $f2, $f3
	addg/uc $f1, $f2, $f3
	addg/s $f1, $f2, $f3
	addg/sc $f1, $f2, $f3
	addg/su $f1, $f2, $f3
	addg/suc $f1, $f2, $f3
	subg $f1, $f2, $f3
	subg/c $f1, $f2, $f3
	subg/u $f1, $f2, $f3
	subg/uc $f1, $f2, $f3
	subg/s $f1, $f2, $f3
	subg/sc $f1, $f2, $f3
	subg/su $f1, $f2, $f3
	subg/suc $f1, $f2, $f3
	mulg $f1, $f2, $f3
	mulg/c $f1, $f2, $f3
	mulg/u $f1, $f2, $f3
	mulg/uc $f1, $f2, $f3
	mulg/s $f1, $f2, $f3
	mulg/sc $f1, $f2, $f3
	mulg/su $f1, $f2, $f3
	mulg/suc $f1, $f2, $f3
	divg $f1, $f2, $f3
	divg/c $f1, $f2, $f3
	divg/u $f1, $f2, $f3
	divg/uc $f1, $f2, $f3
	divg/s $f1, $f2, $f3
	divg/sc $f1, $f2, $f3
	divg/su $f1, $f2, $f3
	divg/suc $f1, $f2, $f3

# CHECK-NEXT: 03 14 22 54 {{.*}}addg $f1, $f2, $f3
# CHECK-NEXT: 03 04 22 54 {{.*}}addg/c $f1, $f2, $f3
# CHECK-NEXT: 03 34 22 54 {{.*}}addg/u $f1, $f2, $f3
# CHECK-NEXT: 03 24 22 54 {{.*}}addg/uc $f1, $f2, $f3
# CHECK-NEXT: 03 94 22 54 {{.*}}addg/s $f1, $f2, $f3
# CHECK-NEXT: 03 84 22 54 {{.*}}addg/sc $f1, $f2, $f3
# CHECK-NEXT: 03 b4 22 54 {{.*}}addg/su $f1, $f2, $f3
# CHECK-NEXT: 03 a4 22 54 {{.*}}addg/suc $f1, $f2, $f3
# CHECK-NEXT: 23 14 22 54 {{.*}}subg $f1, $f2, $f3
# CHECK-NEXT: 23 04 22 54 {{.*}}subg/c $f1, $f2, $f3
# CHECK-NEXT: 23 34 22 54 {{.*}}subg/u $f1, $f2, $f3
# CHECK-NEXT: 23 24 22 54 {{.*}}subg/uc $f1, $f2, $f3
# CHECK-NEXT: 23 94 22 54 {{.*}}subg/s $f1, $f2, $f3
# CHECK-NEXT: 23 84 22 54 {{.*}}subg/sc $f1, $f2, $f3
# CHECK-NEXT: 23 b4 22 54 {{.*}}subg/su $f1, $f2, $f3
# CHECK-NEXT: 23 a4 22 54 {{.*}}subg/suc $f1, $f2, $f3
# CHECK-NEXT: 43 14 22 54 {{.*}}mulg $f1, $f2, $f3
# CHECK-NEXT: 43 04 22 54 {{.*}}mulg/c $f1, $f2, $f3
# CHECK-NEXT: 43 34 22 54 {{.*}}mulg/u $f1, $f2, $f3
# CHECK-NEXT: 43 24 22 54 {{.*}}mulg/uc $f1, $f2, $f3
# CHECK-NEXT: 43 94 22 54 {{.*}}mulg/s $f1, $f2, $f3
# CHECK-NEXT: 43 84 22 54 {{.*}}mulg/sc $f1, $f2, $f3
# CHECK-NEXT: 43 b4 22 54 {{.*}}mulg/su $f1, $f2, $f3
# CHECK-NEXT: 43 a4 22 54 {{.*}}mulg/suc $f1, $f2, $f3
# CHECK-NEXT: 63 14 22 54 {{.*}}divg $f1, $f2, $f3
# CHECK-NEXT: 63 04 22 54 {{.*}}divg/c $f1, $f2, $f3
# CHECK-NEXT: 63 34 22 54 {{.*}}divg/u $f1, $f2, $f3
# CHECK-NEXT: 63 24 22 54 {{.*}}divg/uc $f1, $f2, $f3
# CHECK-NEXT: 63 94 22 54 {{.*}}divg/s $f1, $f2, $f3
# CHECK-NEXT: 63 84 22 54 {{.*}}divg/sc $f1, $f2, $f3
# CHECK-NEXT: 63 b4 22 54 {{.*}}divg/su $f1, $f2, $f3
# CHECK-NEXT: 63 a4 22 54 {{.*}}divg/suc $f1, $f2, $f3

# The G_floating comparisons.  A comparison cannot overflow or underflow, so
# the only trap qualifier is /s, and it takes no rounding letter at all.
	cmpgeq $f1, $f2, $f3
	cmpgeq/s $f1, $f2, $f3
	cmpglt $f1, $f2, $f3
	cmpglt/s $f1, $f2, $f3
	cmpgle $f1, $f2, $f3
	cmpgle/s $f1, $f2, $f3

# CHECK-NEXT: a3 14 22 54 {{.*}}cmpgeq $f1, $f2, $f3
# CHECK-NEXT: a3 94 22 54 {{.*}}cmpgeq/s $f1, $f2, $f3
# CHECK-NEXT: c3 14 22 54 {{.*}}cmpglt $f1, $f2, $f3
# CHECK-NEXT: c3 94 22 54 {{.*}}cmpglt/s $f1, $f2, $f3
# CHECK-NEXT: e3 14 22 54 {{.*}}cmpgle $f1, $f2, $f3
# CHECK-NEXT: e3 94 22 54 {{.*}}cmpgle/s $f1, $f2, $f3

# The conversions between the VAX formats.  D_floating survives on Alpha only
# as these two conversions to and from G_floating.
	cvtdg $f2, $f3
	cvtdg/c $f2, $f3
	cvtdg/u $f2, $f3
	cvtdg/uc $f2, $f3
	cvtdg/s $f2, $f3
	cvtdg/sc $f2, $f3
	cvtdg/su $f2, $f3
	cvtdg/suc $f2, $f3
	cvtgf $f2, $f3
	cvtgf/c $f2, $f3
	cvtgf/u $f2, $f3
	cvtgf/uc $f2, $f3
	cvtgf/s $f2, $f3
	cvtgf/sc $f2, $f3
	cvtgf/su $f2, $f3
	cvtgf/suc $f2, $f3
	cvtgd $f2, $f3
	cvtgd/c $f2, $f3
	cvtgd/u $f2, $f3
	cvtgd/uc $f2, $f3
	cvtgd/s $f2, $f3
	cvtgd/sc $f2, $f3
	cvtgd/su $f2, $f3
	cvtgd/suc $f2, $f3

# CHECK-NEXT: c3 13 e2 57 {{.*}}cvtdg $f2, $f3
# CHECK-NEXT: c3 03 e2 57 {{.*}}cvtdg/c $f2, $f3
# CHECK-NEXT: c3 33 e2 57 {{.*}}cvtdg/u $f2, $f3
# CHECK-NEXT: c3 23 e2 57 {{.*}}cvtdg/uc $f2, $f3
# CHECK-NEXT: c3 93 e2 57 {{.*}}cvtdg/s $f2, $f3
# CHECK-NEXT: c3 83 e2 57 {{.*}}cvtdg/sc $f2, $f3
# CHECK-NEXT: c3 b3 e2 57 {{.*}}cvtdg/su $f2, $f3
# CHECK-NEXT: c3 a3 e2 57 {{.*}}cvtdg/suc $f2, $f3
# CHECK-NEXT: 83 15 e2 57 {{.*}}cvtgf $f2, $f3
# CHECK-NEXT: 83 05 e2 57 {{.*}}cvtgf/c $f2, $f3
# CHECK-NEXT: 83 35 e2 57 {{.*}}cvtgf/u $f2, $f3
# CHECK-NEXT: 83 25 e2 57 {{.*}}cvtgf/uc $f2, $f3
# CHECK-NEXT: 83 95 e2 57 {{.*}}cvtgf/s $f2, $f3
# CHECK-NEXT: 83 85 e2 57 {{.*}}cvtgf/sc $f2, $f3
# CHECK-NEXT: 83 b5 e2 57 {{.*}}cvtgf/su $f2, $f3
# CHECK-NEXT: 83 a5 e2 57 {{.*}}cvtgf/suc $f2, $f3
# CHECK-NEXT: a3 15 e2 57 {{.*}}cvtgd $f2, $f3
# CHECK-NEXT: a3 05 e2 57 {{.*}}cvtgd/c $f2, $f3
# CHECK-NEXT: a3 35 e2 57 {{.*}}cvtgd/u $f2, $f3
# CHECK-NEXT: a3 25 e2 57 {{.*}}cvtgd/uc $f2, $f3
# CHECK-NEXT: a3 95 e2 57 {{.*}}cvtgd/s $f2, $f3
# CHECK-NEXT: a3 85 e2 57 {{.*}}cvtgd/sc $f2, $f3
# CHECK-NEXT: a3 b5 e2 57 {{.*}}cvtgd/su $f2, $f3
# CHECK-NEXT: a3 a5 e2 57 {{.*}}cvtgd/suc $f2, $f3

# G_floating to quadword.  The result is an integer, so the overflow letter is
# v rather than u -- exactly as cvttq's is.
	cvtgq $f2, $f3
	cvtgq/c $f2, $f3
	cvtgq/v $f2, $f3
	cvtgq/vc $f2, $f3
	cvtgq/s $f2, $f3
	cvtgq/sc $f2, $f3
	cvtgq/sv $f2, $f3
	cvtgq/svc $f2, $f3

# CHECK-NEXT: e3 15 e2 57 {{.*}}cvtgq $f2, $f3
# CHECK-NEXT: e3 05 e2 57 {{.*}}cvtgq/c $f2, $f3
# CHECK-NEXT: e3 35 e2 57 {{.*}}cvtgq/v $f2, $f3
# CHECK-NEXT: e3 25 e2 57 {{.*}}cvtgq/vc $f2, $f3
# CHECK-NEXT: e3 95 e2 57 {{.*}}cvtgq/s $f2, $f3
# CHECK-NEXT: e3 85 e2 57 {{.*}}cvtgq/sc $f2, $f3
# CHECK-NEXT: e3 b5 e2 57 {{.*}}cvtgq/sv $f2, $f3
# CHECK-NEXT: e3 a5 e2 57 {{.*}}cvtgq/svc $f2, $f3

# Quadword to a VAX format.  Nothing about this can trap, so there is no trap
# qualifier -- but it still rounds.
	cvtqf $f2, $f3
	cvtqf/c $f2, $f3
	cvtqg $f2, $f3
	cvtqg/c $f2, $f3

# CHECK-NEXT: 83 17 e2 57 {{.*}}cvtqf $f2, $f3
# CHECK-NEXT: 83 07 e2 57 {{.*}}cvtqf/c $f2, $f3
# CHECK-NEXT: c3 17 e2 57 {{.*}}cvtqg $f2, $f3
# CHECK-NEXT: c3 07 e2 57 {{.*}}cvtqg/c $f2, $f3

# The VAX loads and stores.  ldf and stf move an F_floating value into and out
# of the register's VAX layout rather than converting, and ldg and stg move the
# eight bytes of a G_floating value with the longwords in their VAX order.
	ldf $f1, 8($16)
	ldg $f1, 8($16)
	stf $f1, 8($16)
	stg $f1, 8($16)

# CHECK-NEXT: 08 00 30 80 {{.*}}ldf $f1, 8($16)
# CHECK-NEXT: 08 00 30 84 {{.*}}ldg $f1, 8($16)
# CHECK-NEXT: 08 00 30 90 {{.*}}stf $f1, 8($16)
# CHECK-NEXT: 08 00 30 94 {{.*}}stg $f1, 8($16)

# negf and negg are subtractions from zero, as negs and negt are, so they read
# back as the subtraction they encode.
	negf $f2, $f3
	negf/s $f2, $f3
	negg $f2, $f3
	negg/s $f2, $f3

# CHECK-NEXT: 23 10 e2 57 {{.*}}subf $f31, $f2, $f3
# CHECK-NEXT: 23 90 e2 57 {{.*}}subf/s $f31, $f2, $f3
# CHECK-NEXT: 23 14 e2 57 {{.*}}subg $f31, $f2, $f3
# CHECK-NEXT: 23 94 e2 57 {{.*}}subg/s $f31, $f2, $f3

# The qualifiers these instructions do *not* take.  There is no inexact bit, so
# no /sui; the rounding field has only the two VAX modes, so no /m or /d; a
# comparison takes neither an underflow bit nor a rounding letter; a conversion
# from an integer takes no trap qualifier; and the u and v spellings belong to
# the classes whose result is floating and integer respectively.  GNU as rejects
# all nine of these too.
.ifdef BAD
	addf/sui $f1, $f2, $f3
	addf/m $f1, $f2, $f3
	addf/d $f1, $f2, $f3
	addf/v $f1, $f2, $f3
	cmpgeq/su $f1, $f2, $f3
	cmpgeq/c $f1, $f2, $f3
	cvtqf/u $f2, $f3
	cvtgq/u $f2, $f3
	cvtgf/v $f2, $f3
.endif
# BAD: error: invalid floating-point qualifier for this instruction
# BAD: error: invalid floating-point qualifier for this instruction
# BAD: error: invalid floating-point qualifier for this instruction
# BAD: error: invalid floating-point qualifier for this instruction
# BAD: error: invalid floating-point qualifier for this instruction
# BAD: error: invalid floating-point qualifier for this instruction
# BAD: error: invalid floating-point qualifier for this instruction
# BAD: error: invalid floating-point qualifier for this instruction
# BAD: error: invalid floating-point qualifier for this instruction
