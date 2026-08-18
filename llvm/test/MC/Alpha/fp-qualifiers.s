# RUN: llvm-mc -triple=alpha-unknown-linux-gnu --show-encoding %s | FileCheck %s
## An assembler flag must not change what an assembly file means.  Every
## encoding below is the same with and without -mattr, and each is what GNU as
## 2.46.1 produces for the same line.
# RUN: llvm-mc -triple=alpha-unknown-linux-gnu -mattr=+ieee --show-encoding %s \
# RUN:   | FileCheck %s
# RUN: llvm-mc -triple=alpha-unknown-linux-gnu -mattr=+ieee-with-inexact,+fpround-dynamic \
# RUN:   --show-encoding %s | FileCheck %s

## Assembling and disassembling has to come back to what was written, whatever
## -mattr says, because the qualifier is in the bits either way.
# RUN: llvm-mc -triple=alpha-unknown-linux-gnu -filetype=obj %s \
# RUN:   | llvm-objdump -d --mattr=+ieee - | FileCheck %s --check-prefix=DIS

## Unqualified.  This is the case that mattered: with -mieee the assembler used
## to turn it into addt/su, so `clang -mieee -c foo.S' rewrote hand-written
## assembly.  The kernel and libm deliberately mix qualified and unqualified
## operates.
# CHECK: addt $f16, $f17, $f0
# CHECK-SAME: encoding: [0x00,0x14,0x11,0x5a]
# DIS: addt $f16, $f17, $f0
	addt $f16, $f17, $f0

## Software completion, with and without inexact.
# CHECK: addt/su $f16, $f17, $f0
# CHECK-SAME: encoding: [0x00,0xb4,0x11,0x5a]
# DIS: addt/su $f16, $f17, $f0
	addt/su $f16, $f17, $f0
# CHECK: addt/sui $f16, $f17, $f0
# CHECK-SAME: encoding: [0x00,0xf4,0x11,0x5a]
# DIS: addt/sui $f16, $f17, $f0
	addt/sui $f16, $f17, $f0

## A rounding letter may follow the trap qualifier, or stand alone.
# CHECK: addt/sud $f16, $f17, $f0
# CHECK-SAME: encoding: [0x00,0xbc,0x11,0x5a]
# DIS: addt/sud $f16, $f17, $f0
	addt/sud $f16, $f17, $f0
# CHECK: addt/c $f16, $f17, $f0
# CHECK-SAME: encoding: [0x00,0x04,0x11,0x5a]
# DIS: addt/c $f16, $f17, $f0
	addt/c $f16, $f17, $f0

## A compare takes the trap qualifier but no rounding.
# CHECK: cmpteq/su $f16, $f17, $f0
# CHECK-SAME: encoding: [0xa0,0xb4,0x11,0x5a]
# DIS: cmpteq/su $f16, $f17, $f0
	cmpteq/su $f16, $f17, $f0

## Float-to-integer spells its overflow qualifier v rather than u.
# CHECK: cvttq/sv $f16, $f0
# CHECK-SAME: encoding: [0xe0,0xb5,0xf0,0x5b]
# DIS: cvttq/sv $f16, $f0
	cvttq/sv $f16, $f0

## cvttq with no qualifier rounds to nearest; the chopped form is what C needs
## and what codegen emits.
# CHECK: cvttq $f16, $f0
# CHECK-SAME: encoding: [0xe0,0x15,0xf0,0x5b]
# DIS: cvttq $f16, $f0
	cvttq $f16, $f0
# CHECK: cvttq/c $f16, $f0
# CHECK-SAME: encoding: [0xe0,0x05,0xf0,0x5b]
# DIS: cvttq/c $f16, $f0
	cvttq/c $f16, $f0
