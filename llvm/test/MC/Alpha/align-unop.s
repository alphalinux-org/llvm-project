# RUN: llvm-mc -triple=alpha-unknown-linux-gnu -filetype=obj %s -o %t.o
# RUN: llvm-objdump -d %t.o | FileCheck %s

## Code alignment is padded with unop (ldq_u $31, 0($30)), the filler GNU as
## uses, rather than with nop.

# CHECK:      <.text>:
# CHECK-NEXT: nop
# CHECK-NEXT: ldq_u $31, 0($30)
# CHECK-NEXT: ldq_u $31, 0($30)
# CHECK-NEXT: ldq_u $31, 0($30)
# CHECK-NEXT: nop
        .text
        bis     $31, $31, $31
        .align  4
        bis     $31, $31, $31
