# RUN: llvm-mc -triple=alpha-unknown-linux-gnu --show-encoding %s | FileCheck %s
# RUN: llvm-mc -triple=alpha-unknown-linux-gnu -filetype=obj %s | llvm-objdump -d - | FileCheck %s --check-prefix=DISASM

# GAS-compatible pseudo-instructions and assembler mode pragmas used by
# hand-written assembly (context save/restore).  The .set mode pragmas are
# accepted and ignored.

.set noat
# CHECK: bis $31, $16, $0                # encoding: [0x00,0x04,0xf0,0x47]
        mov     $16, $0
# CHECK: bis $31, $31, $1                # encoding: [0x01,0x04,0xff,0x47]
        clr     $1
# CHECK: nop                             # encoding: [0x1f,0x04,0xff,0x47]
        nop
.set at

# The floating-point control register moves, encoded with the register in all
# three fields, round-trip through the disassembler.
# CHECK: mf_fpcr $f0                      # encoding: [0xa0,0x04,0x00,0x5c]
# DISASM: mf_fpcr $f0
        mf_fpcr $f0
# CHECK: mt_fpcr $f0                      # encoding: [0x80,0x04,0x00,0x5c]
# DISASM: mt_fpcr $f0
        mt_fpcr $f0

# The full canonical forms of ret and jmp assemble to the bare words.
# CHECK: ret                             # encoding: [0x01,0x80,0xfa,0x6b]
        ret     $31, ($26), 1
# CHECK: jmp $31, ($27), 0               # encoding: [0x00,0x00,0xfb,0x6b]
        jmp     $31, ($27), 0
