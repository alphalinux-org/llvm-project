# REQUIRES: alpha
## R_ALPHA_LITERAL loads a symbol's address from a GOT entry addressed by gp.
## The assembler rewrites references to local symbols as section symbol plus
## addend, so entries are keyed by the pair and not by the symbol alone.
# RUN: llvm-mc -filetype=obj -triple=alpha-unknown-linux-gnu %s -o %t.o
# RUN: ld.lld -T %S/Inputs/alpha-gp.script %t.o -o %t
# RUN: llvm-objdump -d --no-show-raw-insn %t | FileCheck %s
# RUN: llvm-objdump -s -j .got %t | FileCheck --check-prefix=GOT %s

## gp is .got + 0x8000 = 0x120018000. Entries are allocated in the order they
## are referenced, and the repeated reference to a reuses its entry.
# CHECK:      120000000: ldq $1, -32768($29)
# CHECK-NEXT: 120000004: ldq $2, -32760($29)
# CHECK-NEXT: 120000008: ldq $3, -32768($29)
# CHECK-NEXT: 12000000c: ldq $4, -32752($29)
# CHECK-NEXT: 120000010: ldq $5, -32744($29)

## .data is at 0x120010020, so a is 0x120010020 and loc2 is 0x120010038.
# GOT:      Contents of section .got:
# GOT-NEXT: 120010000 20000120 01000000 28000120 01000000
# GOT-NEXT: 120010010 30000120 01000000 38000120 01000000

.text
.globl _start
_start:
  ldq $1, a($29)        !literal
  ldq $2, b($29)        !literal
  ldq $3, a($29)        !literal
  ldq $4, loc($29)      !literal
  ldq $5, loc2($29)     !literal
  ret

.data
.globl a
a:
  .quad 0
.globl b
b:
  .quad 0
loc:
  .quad 0
loc2:
  .quad 0
