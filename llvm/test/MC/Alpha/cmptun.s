# RUN: llvm-mc -triple=alpha-unknown-linux-gnu -filetype=obj %s \
# RUN:   | llvm-objdump -d - | FileCheck %s

# cmptun is the unordered compare -- the NaN test -- and it is by a wide margin
# the most common instruction in a disassembly of the platform's own libraries
# that this target could not read: 871 occurrences across libc.so.6.1,
# libm.so.6.1, ld-linux.so.2 and libgcc_s.so.1, every one of them cmptun/su.
# Nothing selects it, so the assembler and the disassembler are the whole of
# what there is to check.
#
# cmpteq is here as the neighbour that was already right: cmptun's function
# code is 0x0a4 and cmpteq's is 0x0a5, so a one-bit slip in either direction is
# an instruction that exists, and only a check that names both catches it.

	.text
	cmptun    $f0, $f1, $f2
	cmptun/su $f0, $f1, $f2
	cmpteq    $f0, $f1, $f2

# All three are byte-identical to GNU as 2.46.1.  cmptun: func=0x0a4 ->
# 0x58011482; cmptun/su: func=0x5a4 -> 0x5801b482, the /su bits added by the
# encoder rather than baked into a def of its own.
# CHECK:      82 14 01 58 {{.*}}cmptun $f0, $f1, $f2
# CHECK-NEXT: 82 b4 01 58 {{.*}}cmptun/su $f0, $f1, $f2
# CHECK-NEXT: a2 14 01 58 {{.*}}cmpteq $f0, $f1, $f2
