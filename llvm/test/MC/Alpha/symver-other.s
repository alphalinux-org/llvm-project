# RUN: llvm-mc -triple=alpha-unknown-linux-gnu -filetype=obj %s \
# RUN:   | llvm-readelf -s - | FileCheck %s

# Verify that STO_ALPHA_STD_GPLOAD (0x88) propagates through .set alias chains
# when .symver creates a versioned symbol.  glibc uses strong_alias
# (.set alias, original) followed by compat_symbol (.symver alias, sym@VER)
# for compat symbols; both versioned aliases must carry the st_other bits.

	.text
	.globl __fn
	.ent __fn
__fn:
	.prologue 1
	ret ($26)
	.end __fn

	.globl __old_fn
	.set __old_fn, __fn
	.symver __old_fn, fn@VER_OLD
	.symver __fn, fn@@VER_NEW

# CHECK: FUNC    GLOBAL DEFAULT [<other: 0x88>] {{[0-9]+}} fn@VER_OLD
# CHECK: FUNC    GLOBAL DEFAULT [<other: 0x88>] {{[0-9]+}} fn@@VER_NEW
