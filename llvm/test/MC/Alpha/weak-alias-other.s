# RUN: llvm-mc -triple=alpha-unknown-linux-gnu -filetype=obj %s \
# RUN:   | llvm-readelf -s - | FileCheck %s

# Verify that:
# - .ent marks the symbol as STT_FUNC (matching GAS behavior)
# - .prologue 0 (STO_ALPHA_NOPV, 0x80) and .prologue 1 (STO_ALPHA_STD_GPLOAD,
#   0x88) set the appropriate st_other bits
# - Both bits propagate to weak aliases (glibc's weak_alias / syscall-stub
#   pattern: ".weak foo; foo = __foo")

	.text
	.globl __bar
	.ent __bar
__bar:
	.prologue 0
	ret ($26)
	.end __bar

	.weak bar
	bar = __bar

	.globl __baz
	.ent __baz
__baz:
	ldgp $29, 0($27)
	.prologue 1
	ret ($26)
	.end __baz

	.weak baz
	baz = __baz

# CHECK: FUNC    GLOBAL DEFAULT [<other: 0x80>] {{[0-9]+}} __bar
# CHECK: FUNC    WEAK   DEFAULT [<other: 0x80>] {{[0-9]+}} bar
# CHECK: FUNC    GLOBAL DEFAULT [<other: 0x88>] {{[0-9]+}} __baz
# CHECK: FUNC    WEAK   DEFAULT [<other: 0x88>] {{[0-9]+}} baz

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
