# RUN: llvm-mc -triple=alpha-unknown-linux-gnu -filetype=obj %s -o %t.o
# RUN: llvm-objdump -dr %t.o | FileCheck %s

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
