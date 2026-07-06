# RUN: llvm-mc -triple=alpha-unknown-linux-gnu -show-encoding %s | FileCheck %s

## GNU as accepts an immediate source for mov, materializing it in code rather
## than copying a register, and spells integer register $N as "$rN" too.

# CHECK: lda $4, 5                        # encoding: [0x05,0x00,0x9f,0x20]
        mov     5, $4
# CHECK: bis $31, $1, $2                  # encoding: [0x02,0x04,0xe1,0x47]
        mov     $r1, $r2


## One immediate and one register pair said nothing about the range or the
## mixed spellings.  Zero, a negative value and the top of the 16-bit field all
## take the lda form, and $rN may appear on either side of a register move.
# CHECK: lda $0, 0                        # encoding: [0x00,0x00,0x1f,0x20]
        mov     0, $0
# CHECK: lda $9, -1                       # encoding: [0xff,0xff,0x3f,0x21]
        mov     -1, $9
# CHECK: lda $31, 32767                   # encoding: [0xff,0x7f,0xff,0x23]
        mov     32767, $r31
# CHECK: bis $31, $0, $1                  # encoding: [0x01,0x04,0xe0,0x47]
        mov     $r0, $1
# CHECK: bis $31, $2, $3                  # encoding: [0x03,0x04,0xe2,0x47]
        mov     $2, $r3
