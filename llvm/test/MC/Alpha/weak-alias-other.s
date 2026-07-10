# RUN: llvm-mc -triple=alpha-unknown-linux-gnu -filetype=obj %s \
# RUN:   | llvm-readelf -s - | FileCheck %s

# Verify that STO_ALPHA_NOPV (0x80) propagates to weak aliases created via
# ".weak alias; alias = original" (as used by glibc's weak_alias macro for
# syscall stub symbols such as chdir = __chdir).

	.text
	.globl __bar
	.ent __bar
__bar:
	.prologue 0
	ret ($26)
	.end __bar

	.weak bar
	bar = __bar

# CHECK: NOTYPE  GLOBAL DEFAULT [<other: 0x80>] {{[0-9]+}} __bar
# CHECK: NOTYPE  WEAK   DEFAULT [<other: 0x80>] {{[0-9]+}} bar

## The bits are inherited through a chain of assignments, not just one hop.
	.globl chain_fn
	.ent chain_fn
chain_fn:
	.prologue 1
	ret ($26)
	.end chain_fn

	.globl link1
	.globl link2
	.globl link3
	.set link1, chain_fn
	.set link2, link1
	.set link3, link2

# CHECK: [<other: 0x88>] {{[0-9]+}} link3
