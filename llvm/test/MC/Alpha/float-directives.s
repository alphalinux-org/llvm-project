# RUN: llvm-mc -triple=alpha-unknown-linux-gnu -filetype=obj %s \
# RUN:   | llvm-objdump -s -j .text - | FileCheck %s
# RUN: llvm-mc -triple=alpha-unknown-linux-gnu -filetype=obj %s \
# RUN:   | llvm-objdump -s -j .data - | FileCheck %s --check-prefix=DATA
# RUN: not llvm-mc -triple=alpha-unknown-linux-gnu -filetype=obj \
# RUN:   --defsym ERR=1 %s 2>&1 | FileCheck %s --check-prefix=ERR

# .s_floating and .t_floating emit IEEE single and double values.  Each aligns
# to its own width first, as GNU as does -- the unop at 0x0c below is that
# padding ahead of the first .t_floating.
#
# The fill follows the section, as it does in GNU as: unop in .text, zeros in
# a data section.  A float constant normally lives in .rodata or .data, and
# padding those with unop writes an instruction into the data.

# CHECK:      0000 00000040 0000c0bf 00000000 0000fe2f
# CHECK-NEXT: 0010 00000000 00000040 00000000 0000f8bf
# CHECK-NEXT: 0020 112d4454 fb210940 9c750088 3ce4377e
# CHECK-NEXT: 0030 00000000 00000040

	.s_floating 2.0
	.s_floating -1.5, 0.0
	.t_floating 2.0
	.t_floating -1.5, 3.14159265358979, 1e300
# An integer operand is taken as a floating-point value, not reinterpreted.
	.t_floating 2

# DATA:      0000 01000000 00000000 00000000 00000040
	.data
	.byte 1
	.t_floating 2.0

.ifdef ERR
# ERR: [[#@LINE+1]]:14: error: expected floating-point number
	.t_floating foo

# A hexadecimal integer is not a hexadecimal float: APFloat's hex form needs an
# exponent (0x1p4), so a bare 0x10 is rejected rather than read as sixteen.
# ERR: [[#@LINE+1]]:14: error: invalid floating-point number
	.s_floating 0x10
.endif
