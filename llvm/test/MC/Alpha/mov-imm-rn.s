# RUN: llvm-mc -triple=alpha-unknown-linux-gnu -show-encoding %s | FileCheck %s
# RUN: not llvm-mc -triple=alpha-unknown-linux-gnu -filetype=obj \
# RUN:   --defsym ERR=1 %s -o /dev/null 2>&1 | FileCheck %s --check-prefix=ERR

## GNU as accepts an immediate source for mov, and defines it as `bis $31, imm,
## $Rc' -- the operate instruction's own 8-bit unsigned literal, not the ldi
## materialization macro.  Every encoding below is what GNU as 2.46.1 produces.
## It also spells integer register $N as "$rN".

# CHECK: bis $31, 5, $4                   # encoding: [0x04,0xb4,0xe0,0x47]
        mov     5, $4
# CHECK: bis $31, $1, $2                  # encoding: [0x02,0x04,0xe1,0x47]
        mov     $r1, $r2

## The ends of the literal field, and $rN on either side of a register move.
# CHECK: bis $31, 0, $0                   # encoding: [0x00,0x14,0xe0,0x47]
        mov     0, $0
# CHECK: bis $31, 255, $1                 # encoding: [0x01,0xf4,0xff,0x47]
        mov     255, $r1
# CHECK: bis $31, $0, $1                  # encoding: [0x01,0x04,0xe0,0x47]
        mov     $r0, $1
# CHECK: bis $31, $2, $3                  # encoding: [0x03,0x04,0xe2,0x47]
        mov     $2, $r3

.ifdef ERR
## Outside that field it is an error, as it is in GNU as, rather than a longer
## sequence: the ldi macro is a different mnemonic.
# ERR: error: operand out of range (256 is not between 0 and 255)
        mov     256, $3
# ERR: error: operand out of range (-1 is not between 0 and 255)
        mov     -1, $3
.endif
