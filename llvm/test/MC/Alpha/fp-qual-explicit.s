# RUN: llvm-mc -triple=alpha-unknown-linux-gnu -filetype=obj %s \
# RUN:   | llvm-objdump -d - | FileCheck %s

# Explicit FP qualifiers used in hand-written assembly under -mieee:
#   cvttq/svid  T-float -> integer, software-completion + overflow + inexact + dynamic
#   cvtqt/d     integer -> T-float, dynamic rounding mode
#   cvtst/s     S-float -> T-float, software-completion
# These are assembler-only forms with baked-in function codes; the code emitter
# does not add further trap/rounding bits (TrapClass=0).

	.text
	cvttq/svid $f1, $f0
	cvtqt/d    $f0, $f1
	cvtst/s    $f0, $f1

# cvttq/svid: func=0x7ef -> encoding 0x5be1fde0 (little-endian bytes: e0 fd e1 5b)
# cvtqt/d:   func=0x0fe -> encoding 0x5be01fc1 (little-endian bytes: c1 1f e0 5b)
# cvtst/s:   func=0x6ac -> encoding 0x5be0d581 (little-endian bytes: 81 d5 e0 5b)
# CHECK: e0 fd e1 5b
# CHECK: c1 1f e0 5b
# CHECK: 81 d5 e0 5b
