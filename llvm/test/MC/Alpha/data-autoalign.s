# RUN: llvm-mc -triple=alpha-unknown-linux-gnu -filetype=obj %s -o %t.o
# RUN: llvm-objdump -s -j .data -j .packed %t.o | FileCheck %s
# RUN: llvm-readelf -r %t.o | FileCheck %s --check-prefix=RELOC

## GNU as aligns every data item to its own width before emitting it
## (alpha_cons_align, gas/config/tc-alpha.c), and hand-written Alpha assembly
## leans on it: a .quad table written after an .asciz is expected to start on
## an 8-byte boundary, and an ldq from one that does not faults.  Checked
## byte-for-byte against alpha-unknown-linux-gnu-as 2.46.1.

	.data
	.byte	1
	.short	2
	.byte	3
	.long	4
	.byte	5
	.quad	6
	.byte	7
	.t_floating 1.5
	.byte	8
	.s_floating 2.5
	.asciz	"abc"
	.quad	9

## `.align 0' turns it off until the next `.align N' or section change, so the
## quad below packs against the byte.
	.align	0
	.byte	1
	.quad	10

# CHECK:      Contents of section .data:
# CHECK-NEXT: 0000 01000200 03000000 04000000 05000000
# CHECK-NEXT: 0010 06000000 00000000 07000000 00000000
# CHECK-NEXT: 0020 00000000 0000f83f 08000000 00002040
# CHECK-NEXT: 0030 61626300 00000000 09000000 00000000
# CHECK-NEXT: 0040 010a0000 00000000 00

## The .2byte/.4byte/.8byte spellings are the explicitly unaligned ones -- gas
## maps them to s_alpha_ucons, "Dwarf wants these versions of unaligned" -- and
## clang's DWARF is written with them, so they must stay packed.
	.section .packed,"a",@progbits
	.byte	1
	.2byte	2
	.4byte	3
	.8byte	4

# CHECK:      Contents of section .packed:
# CHECK-NEXT: 0000 01020003 00000004 00000000 000000

## A .gprel32 is a four-byte item and aligns like one.
	.section .tbl,"a",@progbits
	.byte	1
	.gprel32 target
	.text
target:
	ret

# RELOC: 0000000000000004 {{.*}} R_ALPHA_GPREL32
