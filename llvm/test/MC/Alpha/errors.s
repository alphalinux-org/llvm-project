# RUN: not llvm-mc -triple=alpha-unknown-linux-gnu -filetype=obj %s -o /dev/null \
# RUN:   2>&1 | FileCheck %s

## A field too narrow for its value is diagnosed rather than truncated, and a
## malformed operand is rejected before anything reads it as the kind it is
## not.  GNU as diagnoses all of these.

	.text
.ifndef LAYOUT

	jsr $26, $27
# CHECK: [[#@LINE-1]]:2: error: invalid operand for instruction

## The 16-bit signed displacement of a memory-format instruction.
	ldq $1, 40000($30)
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

.endif

## The same fields filled in at layout time by a fixup rather than written
## literally.  These are only reached once the file parses, so they get a run
## of their own.
# RUN: not llvm-mc -triple=alpha-unknown-linux-gnu -filetype=obj --defsym LAYOUT=1 \
# RUN:   %s -o /dev/null 2>&1 | FileCheck %s --check-prefix=LAYOUT
.ifdef LAYOUT
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
