## st_other is target-specific: it holds STO_MIPS_MICROMIPS, the PPC64
## local-entry offset, STO_RISCV_VARIANT_CC and STO_AARCH64_VARIANT_PCS.  An
## alias must not acquire any of those on its own, so the inheritance an alias
## does get is gated on a per-target mask that defaults to none.  Only alpha
## sets it, where the bits describe the gp-load prologue a caller may skip.
##
## RISC-V is the case to pin: variant-CC tells the linker the symbol uses a
## non-standard calling convention, and an alias silently claiming it is an ABI
## bug rather than a cosmetic one.

# REQUIRES: riscv-registered-target
# RUN: llvm-mc -triple riscv64 -filetype=obj %s -o %t.o
# RUN: llvm-readobj --symbols %t.o | FileCheck %s

	.text
	.globl target
	.type target,@function
	.variant_cc target
target:
	ret

	.globl alias
alias = target

## The named symbol keeps its own bits.
# CHECK:      Name: target
# CHECK:      Other [ (0x80)
# CHECK-NEXT:   STO_RISCV_VARIANT_CC
# CHECK-NEXT: ]

## The alias does not inherit them.
# CHECK:      Name: alias
# CHECK:      Other: 0
