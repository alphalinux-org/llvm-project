# RUN: llvm-mc -triple=alpha-unknown-linux-gnu -filetype=obj %s \
# RUN:   | llvm-readelf -s - | FileCheck %s

# .usepv sym, no  sets STO_ALPHA_NOPV    (st_other = 0x80)
# .usepv sym, std sets STO_ALPHA_STD_GPLOAD (st_other = 0x88)

	.text
foo:
	.usepv foo, no
	ret ($26)
bar:
	.usepv bar, std
	ret ($26)

# CHECK: NOTYPE  LOCAL  DEFAULT [<other: 0x80>] {{[0-9]+}} foo
# CHECK: NOTYPE  LOCAL  DEFAULT [<other: 0x88>] {{[0-9]+}} bar
