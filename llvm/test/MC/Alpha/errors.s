# RUN: not llvm-mc -triple=alpha-unknown-linux-gnu -filetype=obj %s -o /dev/null \
# RUN:   2>&1 | FileCheck %s

## A field too narrow for its value is diagnosed rather than truncated, and a
## malformed operand is rejected before anything reads it as the kind it is
## not.  GNU as diagnoses all of these.

	.text
.ifndef LAYOUT

## The operand kinds are checked, not just their number: reading a register
## operand as a memory one asserts in an assertions build and returns garbage
## without.
	ldgp $29, $27
# CHECK: [[#@LINE-1]]:12: error: expected memory operand of the form disp($reg)

	jsr $26, $27
# CHECK: [[#@LINE-1]]:2: error: invalid operand for instruction

## The 16-bit signed displacement of a memory-format instruction.
	ldq $1, 40000($30)
# CHECK: [[#@LINE-1]]:2: error: displacement out of range

## ldgp's displacement rides in the lda half of the pair, which has the same
## 16-bit field.
	ldgp $29, 65536($27)
# CHECK: [[#@LINE-1]]:2: error: displacement out of range

## The 8-bit unsigned literal of an operate-format instruction.
	addq $3, 300, $3
# CHECK: [[#@LINE-1]]:2: error: literal out of range

## A mnemonic that is not one of ours is not silently ignored.
	frobnicate $1, $2
# CHECK: [[#@LINE-1]]:2: error: unrecognized instruction mnemonic

## The relocation suffix: a name that is not in the table, and no name at all.
	ldq $1, x($29) !bogus
# CHECK: [[#@LINE-1]]:18: error: unknown relocation name
	ldq $1, x($29) !
# CHECK: [[#@LINE-1]]:18: error: expected relocation name

## The sequence number after a second `!` has to be one, and an identifier
## there is an error rather than something consumed and dropped -- that would
## turn `!gpdisp!N` into an unpaired `!gpdisp` carrying no addend.
	ldah $29, 0($27) !gpdisp!lo
# CHECK: [[#@LINE-1]]:27: error: expected sequence number

## A sequence number names one ldah/lda pair, so a third use of it is an error
## rather than the silent start of a second pair.
	ldah $29, 0($27) !gpdisp!1
	lda $29, 0($29) !gpdisp!1
	lda $29, 0($29) !gpdisp!1
# CHECK: [[#@LINE-1]]:27: error: !gpdisp!1 is already paired

## A relocation goes into a field of the encoding, and the specifier is only
## accepted where there is one that fits it.  GNU as reports both of these as
## "invalid relocation for field": an operate instruction has no field at all,
## and a 16-bit GP-relative displacement does not fit a jump's 14-bit hint.
## Saying nothing and dropping what was written leaves an object without the
## relocation the author asked for.
	bis $1, $2, $3 !literal
# CHECK: [[#@LINE-1]]:18: error: invalid relocation for field
	ret $31, ($26), 1 !gprelhigh
# CHECK: [[#@LINE-1]]:21: error: invalid relocation for field

## A !lituse_* names the literal it uses by sequence number, so there has to be
## one; GNU as says so too.
	jsr $26, ($27) !lituse_jsr
# CHECK: [[#@LINE-1]]:18: error: no sequence number after !lituse_jsr

## A floating-point qualifier suffix: the '/' has to be followed by one, and
## only an instruction with a trap/rounding field can carry one at all.
	addt/ $f1, $f2, $f3
# CHECK: [[#@LINE-1]]:6: error: expected qualifier after '/'
	addq/su $1, $2, $3
# CHECK: [[#@LINE-1]]:2: error: instruction does not take a floating-point qualifier

## Nor a combination its own mnemonic does not define.  The function field has
## room for spellings no instruction has, and merging one in produces a word
## that is reserved -- or, for cvtst, one whose residual bits are cvtts, so it
## reads back as a real instruction converting the other way.  GNU as reports
## every one of these as an unknown opcode.
	cvtst/u $f1, $f2
# CHECK: [[#@LINE-1]]:2: error: invalid floating-point qualifier for this instruction
	cvtst/su $f1, $f2
# CHECK: [[#@LINE-1]]:2: error: invalid floating-point qualifier for this instruction
	cvtst/c $f1, $f2
# CHECK: [[#@LINE-1]]:2: error: invalid floating-point qualifier for this instruction
## A compare takes the bare form or /su and nothing else -- no rounding letter,
## no underflow bit of its own.
	cmpteq/c $f1, $f2, $f3
# CHECK: [[#@LINE-1]]:2: error: invalid floating-point qualifier for this instruction
	cmpteq/u $f1, $f2, $f3
# CHECK: [[#@LINE-1]]:2: error: invalid floating-point qualifier for this instruction
	cmpteq/sui $f1, $f2, $f3
# CHECK: [[#@LINE-1]]:2: error: invalid floating-point qualifier for this instruction
## An integer-to-floating convert has no underflow bit without inexact.
	cvtqt/u $f1, $f2
# CHECK: [[#@LINE-1]]:2: error: invalid floating-point qualifier for this instruction
	cvtqt/su $f1, $f2
# CHECK: [[#@LINE-1]]:2: error: invalid floating-point qualifier for this instruction
## v is the integer-overflow spelling and belongs to the floating-to-integer
## converts alone; u is everything else's.  They encode the same bits, so only
## the spelling tells them apart.
	addt/sv $f1, $f2, $f3
# CHECK: [[#@LINE-1]]:2: error: invalid floating-point qualifier for this instruction
	cvttq/su $f1, $f2
# CHECK: [[#@LINE-1]]:2: error: invalid floating-point qualifier for this instruction

## .usepv takes a symbol and one of two modes, and says so for each way of
## getting it wrong.
	.usepv
# CHECK: [[#@LINE-1]]:2: error: expected symbol name after .usepv
	.usepv foo
# CHECK: [[#@LINE-1]]:12: error: expected ',' after symbol name
	.usepv foo, bogus
# CHECK: [[#@LINE-1]]:19: error: unknown .usepv mode 'bogus'
	.usepv foo, 42
# CHECK: [[#@LINE-1]]:14: error: expected 'std' or 'no'

.endif

## The same fields filled in at layout time by a fixup rather than written
## literally.  These are only reached once the file parses, so they get a run
## of their own.
# RUN: not llvm-mc -triple=alpha-unknown-linux-gnu -filetype=obj --defsym LAYOUT=1 \
# RUN:   %s -o /dev/null 2>&1 | FileCheck %s --check-prefix=LAYOUT
.ifdef LAYOUT
## A symbol in a memory displacement with no relocation suffix has nothing to
## fill it in: the 16-bit field takes no relocation of its own, so whatever is
## written there has to fold to a constant.  Accepting it silently would leave
## a zero displacement and no relocation to correct it.
	ldq $1, x($29)
# LAYOUT: [[#@LINE-1]]:10: error: expression is not an assembly-time constant

0:
	lda $1, 99f-0b($26)
# LAYOUT: [[#@LINE-1]]:13: error: displacement out of range
	addq $3, 99f-0b, $3
# LAYOUT: [[#@LINE-1]]:14: error: literal out of range
	.space 100000
99:

## A branch displacement is 21 bits of instruction units: +/- 4 MiB, and only
## to a 4-byte boundary.
	br $31, 97f
# LAYOUT: [[#@LINE-1]]:10: error: branch target out of range
	.space 4200000
97:
	br $31, 96f
# LAYOUT: [[#@LINE-1]]:10: error: branch target must be 4-byte aligned
	.byte 0
96:
.endif
