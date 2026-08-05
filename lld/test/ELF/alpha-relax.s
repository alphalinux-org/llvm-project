# REQUIRES: alpha
## --relax rewrites a call made through the GOT into a direct branch when the
## callee is close enough to reach. The GOT load can only be dropped as well
## when the callee advertises the standard gp load, since the call then enters
## it past that load and it never looks at $27.
# RUN: llvm-mc -filetype=obj -triple=alpha-unknown-linux-gnu %s -o %t.o
# RUN: ld.lld -T %S/Inputs/alpha-gp.script --defsym far=0x130000000 %t.o -o %t
# RUN: llvm-objdump -d --no-show-raw-insn %t | FileCheck %s
# RUN: ld.lld -T %S/Inputs/alpha-gp.script --defsym far=0x130000000 --no-relax %t.o -o %t.norelax
# RUN: llvm-objdump -d --no-show-raw-insn %t.norelax | FileCheck --check-prefix=NORELAX %s

	.text
	.globl _start
_start:
## A call to a local function. The branch is direct, but the load stays: this
## callee has no advertised gp load, so it still derives its gp from $27.
# CHECK:      120000000: ldq $27, -32768($29)
## local - (0x120000004 + 4) = 0x28, >> 2 = 10.
# CHECK-NEXT: 120000004: bsr $26, 10
	ldq $27, local($29)	!literal
.Lcall:
	jsr $26, ($27)
	.reloc .Lcall, R_ALPHA_LITUSE, 3

## gpload does advertise it, so the load becomes a nop and the branch skips the
## two instructions that establish the callee's gp.
# CHECK-NEXT: 120000008: ldq_u $31, 0($30)
## gpload + 8 - (0x12000000c + 4) = 0x2c, >> 2 = 11.
# CHECK-NEXT: 12000000c: bsr $26, 11
	ldq $27, gpload($29)	!literal
.Lgpload:
	jsr $26, ($27)
	.reloc .Lgpload, R_ALPHA_LITUSE, 3

## The same callee, but the loaded address is also used as a base register, so
## the load has to stay and the call has to enter at the top.
# CHECK-NEXT: 120000010: ldq $27, -32760($29)
# CHECK-NEXT: 120000014: ldl $1, 0($27)
## gpload - (0x120000018 + 4) = 0x18, >> 2 = 6.
# CHECK-NEXT: 120000018: bsr $26, 6
	ldq $27, gpload($29)	!literal
.Lbase:
	ldl $1, 0($27)
	.reloc .Lbase, R_ALPHA_LITUSE, 1
.Lbasecall:
	jsr $26, ($27)
	.reloc .Lbasecall, R_ALPHA_LITUSE, 3

## Out of the branch's reach, so nothing changes.
# CHECK-NEXT: 12000001c: ldq $27, -32752($29)
# CHECK-NEXT: 120000020: jsr $26, ($27)
	ldq $27, far($29)	!literal
.Lfar:
	jsr $26, ($27)
	.reloc .Lfar, R_ALPHA_LITUSE, 3

## A tail call is the same sequence with a jmp in place of the jsr, and becomes
## the br that likewise discards the return address.
# CHECK-NEXT: 120000024: ldq_u $31, 0($30)
## gpload + 8 - (0x120000028 + 4) = 0x10, >> 2 = 4.
# CHECK-NEXT: 120000028: br $31, 4
	ldq $27, gpload($29)	!literal
.Ltail:
	jmp $31, ($27), 0
	.reloc .Ltail, R_ALPHA_LITUSE, 3

	ret

	.globl local
local:
	ret

	.globl gpload
	.usepv gpload, std
gpload:
	ldgp $29, 0($27)
	ret

## Without --relax every call keeps its GOT load and its jsr.
# NORELAX:      120000000: ldq $27, -32768($29)
# NORELAX-NEXT: 120000004: jsr $26, ($27)
# NORELAX-NEXT: 120000008: ldq $27, -32760($29)
# NORELAX-NEXT: 12000000c: jsr $26, ($27)
# NORELAX-NEXT: 120000010: ldq $27, -32760($29)
# NORELAX-NEXT: 120000014: ldl $1, 0($27)
# NORELAX-NEXT: 120000018: jsr $26, ($27)
# NORELAX-NEXT: 12000001c: ldq $27, -32752($29)
# NORELAX-NEXT: 120000020: jsr $26, ($27)
# NORELAX-NEXT: 120000024: ldq $27, -32760($29)
# NORELAX-NEXT: 120000028: jmp $31, ($27), 0
