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

## bfd's special-section table gives .sbss and .sdata SHF_ALPHA_GPREL, and
## .sbss SHT_NOBITS, from the name alone; gas fills both in, and gcc's
## -msmall-data output leans on that -- it writes neither the 's' nor a type.

# CHECK:      Name: .sbss
# CHECK:      Type: SHT_NOBITS
# CHECK:      Flags [ (0x10000003)
# CHECK-NEXT:   SHF_ALLOC (0x2)
# CHECK-NEXT:   SHF_ALPHA_GPREL (0x10000000)
# CHECK-NEXT:   SHF_WRITE (0x1)
# CHECK-NEXT: ]

# CHECK:      Name: .sdata
# CHECK:      Type: SHT_PROGBITS
# CHECK:      Flags [ (0x10000003)
# CHECK-NEXT:   SHF_ALLOC (0x2)
# CHECK-NEXT:   SHF_ALPHA_GPREL (0x10000000)
# CHECK-NEXT:   SHF_WRITE (0x1)
# CHECK-NEXT: ]

## A dotted suffix names the same kind of section, as bfd's -2 length says.
# CHECK:      Name: .sbss.x
# CHECK:      Type: SHT_NOBITS
# CHECK:      Flags [ (0x10000003)

# ASM: .section .sbss,"aws",@nobits
# ASM: .section .sdata,"aws",@progbits

	.section .sbss,"aw"
	.zero 8
	.section .sdata,"aw"
	.quad 1
	.section .sbss.x,"aw"
	.zero 4
