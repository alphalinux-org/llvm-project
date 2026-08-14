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
