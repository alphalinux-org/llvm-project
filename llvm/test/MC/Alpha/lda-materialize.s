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

.ifdef SAME
# ERR: needs a scratch register distinct from the base
	lda $2, TASK_SIZE($2)
.endif
