# RUN: not llvm-mc -triple=alpha-unknown-linux-gnu -filetype=obj %s -o /dev/null 2>&1 \
# RUN:   | FileCheck %s

## !samegp is the only relocation specifier a branch displacement takes.  The
## others name relocations that mean nothing in a 21-bit PC-relative field, and
## taking whichever one was written as the fixup kind put it on the branch: this
## assembled to a branch carrying R_ALPHA_LITERAL.

	.text
	br $31, foo		!literal
# CHECK: error: unsupported relocation specifier on a branch target

	bsr $26, foo		!gpdisp
# CHECK: error: unsupported relocation specifier on a branch target
