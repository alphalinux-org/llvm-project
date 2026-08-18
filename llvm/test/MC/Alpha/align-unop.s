# RUN: llvm-mc -triple=alpha-unknown-linux-gnu -filetype=obj %s -o %t.o
# RUN: llvm-objdump -d %t.o | FileCheck %s

## Code alignment is padded the way GNU as's alpha_handle_align pads it: one
## unop (ldq_u $31, 0($30)) if the number of words to fill is odd, then a
## repeating eight-byte nop/unop pair.  The 21064 issues that pair in one
## cycle, which a run of unops does not.  Filling uniformly with unop disagrees
## with GNU as for every odd-word gap -- as here, where three words are filled.

# CHECK:      <.text>:
# CHECK-NEXT: nop
# CHECK-NEXT: ldq_u $31, 0($30)
# CHECK-NEXT: nop
# CHECK-NEXT: ldq_u $31, 0($30)
# CHECK-NEXT: nop
        .text
        bis     $31, $31, $31
        .align  4
        bis     $31, $31, $31
