# RUN: llvm-mc -triple=alpha-unknown-linux-gnu -filetype=obj %s -o /dev/null
# RUN: not llvm-mc -triple=alpha-unknown-linux-gnu -filetype=obj \
# RUN:   --defsym ERR=1 %s 2>&1 | FileCheck %s --check-prefix=ERR

## The names .arch takes, and what each permits, come from GNU as's cpu_types
## table (gas/config/tc-alpha.c).  Two things about it are easy to get wrong:
##
## Its CIX bit gates ctpop/ctlz/cttz and itoft/ftoit/sqrtt alike -- there is no
## separate FIX bit -- so a name granting CIX there has to grant both of our
## features.  And ev6 grants it even though the 21264 part has only the FIX
## half: .arch says what the assembler will accept, which is not the same
## question as what -mcpu says the part implements.

	.arch ev4
	.arch ev45
	.arch lca45
	.arch ev5
	.arch all
	.arch ev56
	ldbu $0, 0($16)

	.arch pca56
	minub8 $16, $17, $0

	.arch ev6
	ctpop $16, $0
	itoft $16, $f0
	sqrtt $f16, $f0

	.arch ev67
	.arch ev68
	ctlz $16, $0

## .arch replaces the instruction set rather than adding to it, the way GNU as
## does, so a narrower name takes the extension instructions away again.
	.arch ev6
	ctpop $16, $0
	.arch ev4

.ifdef ERR
# ERR: [[#@LINE+1]]:2: error: failed to match instruction
	ctpop $16, $0

## A chip number is a -m command-line name, not a .arch name: GNU as reads a
## symbol here, so a leading digit does not parse there either.
# ERR: error: expected architecture name after .arch
	.arch 21264
# ERR: error: unknown Alpha architecture 'ev9'
	.arch ev9
.endif
