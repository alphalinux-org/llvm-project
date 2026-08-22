# RUN: not llvm-mc -triple=alpha-unknown-linux-gnu -filetype=obj %s -o /dev/null 2>&1 \
# RUN:   | FileCheck %s

## !samegp is the only relocation specifier a branch displacement takes.  The
## others fill a 16-bit memory displacement and mean nothing in a 21-bit
## PC-relative field, so they are rejected the way GNU as rejects them; taking
## whichever one was written as the fixup kind put it on the branch, and this
## assembled to a branch carrying R_ALPHA_LITERAL.  The code emitter keeps a
## check of its own for a specifier reaching it from anywhere but the parser.

	.text
	br $31, foo		!literal
# CHECK: error: invalid relocation for field

	bsr $26, foo		!gpdisp
# CHECK: error: invalid relocation for field
