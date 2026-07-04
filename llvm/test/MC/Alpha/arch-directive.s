# RUN: llvm-mc -triple=alpha-unknown-linux-gnu -filetype=obj %s -o /dev/null
# RUN: not llvm-mc -triple=alpha-unknown-linux-gnu -filetype=obj \
# RUN:   --defsym ERR=1 %s 2>&1 | FileCheck %s --check-prefix=ERR

## .arch replaces the instruction set rather than adding to it, so it both
## grants and takes away.  The emission side is covered by
## CodeGen/Alpha/arch-directive.ll; this is the parsing side.

## A name that grants BWX lets a BWX instruction assemble.
	.arch ev56
	ldbu $0, 0($16)

## And a narrower name afterwards takes it away again.
.ifdef ERR
	.arch ev6
	.arch ev4
	ldbu $0, 0($16)
# ERR: error: failed to match instruction

## An unknown name is reported rather than ignored.
	.arch ev9
# ERR: error: unknown Alpha architecture 'ev9'
.endif
