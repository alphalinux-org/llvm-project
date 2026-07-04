# RUN: llvm-mc -triple=alpha-unknown-linux-gnu -show-encoding %s | FileCheck %s

## GNU as spells a register copy as mov and a zeroing as clr; both are bis
## against $31, whose result register is the last operand.

# CHECK: bis $31, $1, $2                  # encoding: [0x02,0x04,0xe1,0x47]
        mov     $1, $2
# CHECK: bis $31, $31, $3                 # encoding: [0x03,0x04,0xff,0x47]
        clr     $3
