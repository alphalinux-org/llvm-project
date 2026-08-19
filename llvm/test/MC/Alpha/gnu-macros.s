# RUN: llvm-mc -triple=alpha-unknown-linux-gnu -filetype=obj %s -o %t.o
# RUN: llvm-objdump -dr %t.o | FileCheck %s
# RUN: llvm-readelf -r -s %t.o | FileCheck %s --check-prefix=RELOC
# RUN: llvm-mc -triple=alpha-unknown-linux-gnu -mattr=+bwx -filetype=obj \
# RUN:   --defsym BWX=1 %s -o /dev/null
# RUN: not llvm-mc -triple=alpha-unknown-linux-gnu -filetype=obj \
# RUN:   --defsym BWX=1 %s 2>&1 | FileCheck %s --check-prefix=NOBWX

# GNU as macros the kernel's hand-written assembly relies on.

# or is GNU as's name for bis.
# CHECK: bis $1, $2, $3
	or $1, $2, $3

# A load from a bare symbol goes through the GOT: load the symbol's address from
# its GOT entry (an R_ALPHA_LITERAL), then dereference at the requested width.
# CHECK:      ldq $0, 0($29)
# CHECK-NEXT: R_ALPHA_LITERAL hwrpb
# CHECK-NEXT: ldq $0, 0($0)
	ldq $0, hwrpb

# A longword load from a symbol derefs with ldl (the GOT slot stays a quadword).
# CHECK:      ldq $4, 0($29)
# CHECK-NEXT: R_ALPHA_LITERAL alpha_using_srm
# CHECK-NEXT: ldl $4, 0($4)
	ldl $4, alpha_using_srm

# lda of a bare symbol is just the address from the GOT (no dereference).
# CHECK:      ldq $8, 0($29)
# CHECK-NEXT: R_ALPHA_LITERAL init_thread_union
	lda $8, init_thread_union

# A call/jump to a bare symbol loads its address into $27 and jumps through it.
# CHECK:      ldq $27, 0($29)
# CHECK-NEXT: R_ALPHA_LITERAL start_kernel
# CHECK-NEXT: jsr $26, ($27)
	jsr $26, start_kernel

# CHECK:      ldq $27, 0($29)
# CHECK-NEXT: R_ALPHA_LITERAL schedule_tail
# CHECK-NEXT: jmp $31, ($27), 0
	jmp $31, schedule_tail

# A `!samegp` branch to a routine sharing the caller's gp emits an R_ALPHA_BRSGP
# relocation; the target must carry the .prologue-derived st_other (here NOPV).
# CHECK: bne $1,
	bne $1, memcpy !samegp

	.ent memcpy
memcpy:
	.prologue 0
	ret ($26)
	.end memcpy

# RELOC: R_ALPHA_BRSGP
# .prologue 0 gives memcpy the STO_ALPHA_NOPV st_other (0x80) the linker checks.
# RELOC: FUNC {{.*}}<other: 0x80>{{.*}} memcpy

	.ent std_gpload
std_gpload:
	.prologue 1
	ret ($26)
	.end std_gpload

# .prologue 1 gives the function the STO_ALPHA_STD_GPLOAD st_other (0x88).
# Bit 3 (0x08) survives: st_other holds the whole byte, not just the three
# visibility bits.
# RELOC: FUNC {{.*}}<other: 0x88>{{.*}} std_gpload

## The byte and word loads the GOT macro expands into are BWX instructions and
## have to be available.  GNU as expands them into an ldq_u/ext pair when the
## target has no BWX; we do not implement that macro, so refuse rather than
## emit an instruction the target cannot execute.  The matcher already refuses
## the `ldbu $0, 0($16)' spelling of the same load.
.ifdef BWX
# NOBWX: error: instruction requires the following: Byte/word extension (BWX)
	ldbu $0, sym
# NOBWX: error: instruction requires the following: Byte/word extension (BWX)
	ldwu $1, sym
.endif
