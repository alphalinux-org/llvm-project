# RUN: llvm-mc -triple=alpha-unknown-linux-gnu -filetype=obj %s -o %t.o
# RUN: llvm-objdump -s -j .data %t.o | FileCheck %s
# RUN: llvm-readobj -r %t.o | FileCheck %s --check-prefix=RELOC

## A .byte, .short, .long or .quad whose value is not known until the section
## is laid out becomes a fixup of that width, and the assembler writes back
## exactly that many bytes.  A fixup narrower or wider than an instruction word
## is not the common case on a fixed-width machine, so nothing else here
## exercises one: everything else this backend fixes up sits in a four-byte
## instruction word.

	.data
a:
## Each fixed-up field is followed by a constant, and the value is wide enough
## that a write of the wrong width would be seen reaching into it.
	.byte	dend-dstart
	.byte	0xaa
	.short	dend-dstart
	.short	0xbbbb
	.long	dend-dstart
	.long	0xcccccccc
## The difference the other way round is negative, and has to sign-extend
## across the whole quadword rather than stopping at the low four bytes.
	.quad	dstart-dend
	.quad	0x1122334455667788

## The distance is not known until the section below is laid out, so each of
## these is a real fixup rather than something folded as it was parsed.
	.bss
dstart:
	.zero	0x11111
dend:

# CHECK:      Contents of section .data:
# CHECK-NEXT: 0000 11aa1111 bbbb1111 0100cccc ccccefee
# CHECK-NEXT: 0010 feffffff ffff8877 66554433 2211

## A symbol the assembler cannot resolve keeps a relocation instead, whose type
## follows the width of the field and whether the reference is pc-relative.
	.section .rel,"a",@progbits
	.quad	undef_q
	.long	undef_l
	.quad	undef_q - .
	.long	undef_l - .

# RELOC:      Section ({{[0-9]+}}) .rela.rel {
# RELOC-NEXT:   0x0 R_ALPHA_REFQUAD undef_q 0x0
# RELOC-NEXT:   0x8 R_ALPHA_REFLONG undef_l 0x0
# RELOC-NEXT:   0xC R_ALPHA_SREL64 undef_q 0x0
# RELOC-NEXT:   0x14 R_ALPHA_SREL32 undef_l 0x0
# RELOC-NEXT: }
