# RUN: llvm-mc -triple=alpha-unknown-linux-gnu -show-encoding %s | FileCheck %s

## GNU as accepts an immediate source for mov, materializing it in code rather
## than copying a register, and spells integer register $N as "$rN" too.

# CHECK: lda $4, 5                        # encoding: [0x05,0x00,0x9f,0x20]
        mov     5, $4
# CHECK: bis $31, $1, $2                  # encoding: [0x02,0x04,0xe1,0x47]
        mov     $r1, $r2
