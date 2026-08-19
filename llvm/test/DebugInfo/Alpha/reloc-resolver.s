## llvm-dwarfdump resolves the relocations in an unlinked object's debug
## sections itself, through RelocationResolver.  Check the Alpha arm of it:
## R_ALPHA_REFQUAD is S + A and R_ALPHA_SREL32 is S + A - P, and a wrong sign
## on either would go unnoticed otherwise.

# RUN: llvm-mc -triple=alpha-unknown-linux-gnu -filetype=obj %s -o %t.o
# RUN: llvm-readobj -r %t.o | FileCheck %s --check-prefix=RELOC
# RUN: llvm-dwarfdump --debug-info %t.o | FileCheck %s

# RELOC: R_ALPHA_REFQUAD target 0x0
# RELOC: R_ALPHA_SREL32 target 0x0

	.text
	.globl	filler
filler:
	ret
	ret
	.globl	target
target:
	ret

	.section	.debug_abbrev,"",@progbits
	.byte	1                       # abbreviation code
	.byte	0x11                    # DW_TAG_compile_unit
	.byte	0                       # DW_CHILDREN_no
	.byte	0x11                    # DW_AT_low_pc
	.byte	0x01                    # DW_FORM_addr
	.byte	0x52                    # DW_AT_entry_pc
	.byte	0x06                    # DW_FORM_data4
	.byte	0                       # end of attributes
	.byte	0
	.byte	0                       # end of abbreviations

	.section	.debug_info,"",@progbits
	.4byte	.Lcu_end-.Lcu_start     # unit length
.Lcu_start:
	.2byte	4                       # DWARF version
	.4byte	0                       # abbrev offset
	.byte	8                       # address size
	.byte	1                       # abbreviation code
## target is eight bytes into .text, so a resolver that dropped the addend or
## the symbol would print zero here.
	.quad	target                  # DW_AT_low_pc, R_ALPHA_REFQUAD
.Lentry:
	.4byte	target-.Lentry          # DW_AT_entry_pc, R_ALPHA_SREL32
	.byte	0                       # end of children
.Lcu_end:

# CHECK: DW_TAG_compile_unit
# CHECK:   DW_AT_low_pc (0x0000000000000008)
## S + A - P = 8 - 0x14, so a resolver that got the sign of the subtraction
## wrong, or left P out, would not print this.
# CHECK:   DW_AT_entry_pc (0xfffffff4)
