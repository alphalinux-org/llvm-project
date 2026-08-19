# RUN: llvm-mc -triple=alpha-unknown-linux-gnu -filetype=obj %s \
# RUN:   | llvm-readelf -r -x .data - | FileCheck %s
## The same input round-trips through assembly output.  The GPREL32 fixup has
## no `!name' suffix -- the directive is the relocation -- so the expression
## printer has to emit the bare symbol rather than assert or drop it.
# RUN: llvm-mc -triple=alpha-unknown-linux-gnu %s | FileCheck %s --check-prefix=ASM
# ASM:      .gprel32 target
# ASM-NEXT: .gprel32 target+4

## `.gprel32 sym' emits a four-byte GP-relative value.  gcc uses it for the
## jump tables it puts in .data, where the entry is the distance from $gp to
## the target rather than an address, so the table needs no dynamic relocation.

	.data
	.align 2
tbl:
	.gprel32 target
	.gprel32 target+4

	.text
target:
	ret $31, ($26), 1

## The assembler resolves the local label against its section, so the
## relocation names .text with the offset in the addend.
# CHECK:      R_ALPHA_GPREL32 {{.*}} .text + 0
# CHECK-NEXT: R_ALPHA_GPREL32 {{.*}} .text + 4
# CHECK: Hex dump of section '.data':
# CHECK-NEXT: 0x00000000 00000000 00000000
