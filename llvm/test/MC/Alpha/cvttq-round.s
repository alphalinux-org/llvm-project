# RUN: llvm-mc -triple=alpha-unknown-linux-gnu -show-encoding %s | FileCheck %s
# RUN: llvm-mc -triple=alpha-unknown-linux-gnu -filetype=obj %s \
# RUN:   | llvm-objdump -d - | FileCheck %s --check-prefix=DIS

## cvttq is the float-to-integer convert, and the rounding letter is part of
## how it is spelled: cvttq/svc is the trapping, chopped form the compiler
## emits under -mieee, and cvttq/sv is the same conversion rounding to nearest.
## The two differ in bits 7:6 of the function field.  The ambient
## -mfp-rounding-mode does not reach this instruction -- C's conversion
## truncates regardless -- so only a written letter selects the field.

	.text
	cvttq/svc $f1, $f2
	cvttq/sv  $f1, $f2
	cvttq/c   $f1, $f2
	cvttq     $f1, $f2

# func 0x52f
# CHECK: cvttq/svc $f1, $f2                      # encoding: [0xe2,0xa5,0xe1,0x5b]
# func 0x5af
# CHECK: cvttq/sv $f1, $f2                       # encoding: [0xe2,0xb5,0xe1,0x5b]
# func 0x02f
# CHECK: cvttq/c $f1, $f2                        # encoding: [0xe2,0x05,0xe1,0x5b]
# func 0x0af
# CHECK: cvttq $f1, $f2                          # encoding: [0xe2,0x15,0xe1,0x5b]

## And each reads back as what was written.
# DIS: cvttq/svc $f1, $f2
# DIS: cvttq/sv $f1, $f2
# DIS: cvttq/c $f1, $f2
# DIS: cvttq $f1, $f2
