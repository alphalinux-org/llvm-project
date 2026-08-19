# RUN: llvm-mc -triple=alpha-unknown-linux-gnu -filetype=obj %s -o %t.o
# RUN: llvm-objdump -dr %t.o | FileCheck %s
# RUN: not llvm-mc -triple=alpha-unknown-linux-gnu --defsym SAME=1 %s 2>&1 | FileCheck %s --check-prefix=ERR

# lda $Rc, disp($Rb) with a symbolic displacement loads the symbol's address
# from the GOT and adds the base.
# CHECK:      ldq $0, 0($29)
# CHECK-NEXT: R_ALPHA_LITERAL TASK_SIZE
# CHECK-NEXT: addq $0, $8, $0
	lda $0, TASK_SIZE($8)

# A constant displacement too wide for the 16-bit field is materialized.
# CHECK:      lda $1, 1024($31)
# CHECK-NEXT: sll $1, 32, $1
# CHECK-NEXT: addq $1, $8, $1
	lda $1, 0x40000000000($8)

# `sym+N' has to have the addend taken out of the literal relocation: the
# linker makes a GOT entry for the symbol, not for the symbol plus a constant.
# CHECK:      ldq $3, 0($29)
# CHECK-NEXT: R_ALPHA_LITERAL TASK_SIZE
# CHECK-NEXT: lda $3, 8($3)
# CHECK-NEXT: addq $3, $8, $3
	lda $3, TASK_SIZE+8($8)

# $31 reads as zero, so no add is emitted for it.
# CHECK:      ldq $4, 0($29)
# CHECK-NEXT: R_ALPHA_LITERAL TASK_SIZE
# CHECK-NOT:  addq $4
	lda $4, TASK_SIZE($31)

.ifdef SAME
# ERR: needs a scratch register distinct from the base
	lda $2, TASK_SIZE($2)
.endif
