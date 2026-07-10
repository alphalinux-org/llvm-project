# RUN: not llvm-mc -triple=x86_64-unknown-linux-gnu %s -o /dev/null 2>&1 \
# RUN:   | FileCheck %s

## eatToEndOfStatement() lexes raw, so it can leave a block comment as the
## current token.  parseStatement() has to skip it, or the comment-only line
## below is taken for the start of a statement and draws a second, spurious
## error.

	.byte 1 2 /* trailing */
/* a line that is nothing but a comment */
	nop

# CHECK: :9:10: error: unexpected token
# CHECK-NOT: error: unexpected token at start of statement
