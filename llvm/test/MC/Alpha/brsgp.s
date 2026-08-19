# RUN: llvm-mc -triple=alpha-unknown-linux-gnu -filetype=obj %s \
# RUN:   | llvm-readobj -r - | FileCheck %s

# R_ALPHA_BRSGP must be symbol-relative (not section+offset) so the linker
# can check st_other of the target symbol for STO_ALPHA_NOPV / STD_GPLOAD.

	.text
foo:
	.usepv foo, no
	ret ($26)
bar:
	bsr $26, foo	!samegp

# CHECK: R_ALPHA_BRSGP foo 0x0
