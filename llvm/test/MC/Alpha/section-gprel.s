# RUN: llvm-mc -triple=alpha-unknown-linux-gnu -filetype=obj %s -o %t.o
# RUN: llvm-readobj -S %t.o | FileCheck %s
# RUN: llvm-mc -triple=alpha-unknown-linux-gnu %s | FileCheck --check-prefix=ASM %s

## The 's' section flag is SHF_ALPHA_GPREL, marking small data that is reached
## with a gp-relative displacement. The Linux kernel's module support emits it.

# CHECK:      Name: .got
# CHECK:      Flags [ (0x10000003)
# CHECK-NEXT:   SHF_ALLOC (0x2)
# CHECK-NEXT:   SHF_ALPHA_GPREL (0x10000000)
# CHECK-NEXT:   SHF_WRITE (0x1)
# CHECK-NEXT: ]

## The flag round-trips through the assembly printer.
# ASM: .section .got,"aws",@progbits

	.section .got,"aws",@progbits
	.align 3
	.previous
	.text
foo:
	ret
