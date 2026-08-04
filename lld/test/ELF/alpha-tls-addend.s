# REQUIRES: alpha
## A GOT-based TLS relocation against sym+N must reach a GOT entry holding the
## offset of sym+N, not of sym.  The entry is keyed on the addend, so a second
## reference with a different addend gets a second entry.
# RUN: llvm-mc -filetype=obj -triple=alpha-unknown-linux-gnu %s -o %t.o
# RUN: ld.lld -T %S/Inputs/alpha-gp.script %t.o -o %t
# RUN: llvm-objdump -d --no-show-raw-insn %t | FileCheck %s
# RUN: llvm-objdump -s -j .got %t | FileCheck --check-prefix=GOT %s

## .tdata has p_align 1, so x is at tp + 16 and x+8 is at tp + 24.  Three
## distinct gottprel entries are needed: x, x+8 and x+16.
# CHECK:      120000000: ldq $1, -32768($29)
# CHECK-NEXT: 120000004: ldq $2, -32760($29)
# CHECK-NEXT: 120000008: ldq $3, -32752($29)
## The same addend as the first reference reuses its entry.
# CHECK-NEXT: 12000000c: ldq $4, -32768($29)
## General dynamic against x+8: the offset half of the pair carries the addend.
# CHECK-NEXT: 120000010: lda $16, -32744($29)

## .got+0x00: tp offset of x      = 16
## .got+0x08: tp offset of x+8    = 24
## .got+0x10: tp offset of x+16   = 32
## .got+0x18: module index 1, dtpoff of x+8 = 8
# GOT:      Contents of section .got:
# GOT-NEXT: 120010000 10000000 00000000 18000000 00000000
# GOT-NEXT: 120010010 20000000 00000000 01000000 00000000
# GOT-NEXT: 120010020 08000000 00000000

.text
.globl _start
_start:
  ldq $1, x($29)        !gottprel
  ldq $2, x+8($29)      !gottprel
  ldq $3, x+16($29)     !gottprel
  ldq $4, x($29)        !gottprel
  lda $16, x+8($29)     !tlsgd
  ret

.section .tdata,"awT",@progbits
.globl x
x:
  .quad 0
  .quad 0
  .quad 0
