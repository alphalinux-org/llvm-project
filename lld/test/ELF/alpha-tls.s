# REQUIRES: alpha
## Test the four TLS models in a non-preemptible, statically linked layout.
## Alpha is TLS variant 1 with a 16-byte TCB, so the main program's TLS block
## starts at tp + round_up(16, p_align). No GD/LD/IE optimization is done
## because the __tls_get_addr call is a separate relocation.
# RUN: llvm-mc -filetype=obj -triple=alpha-unknown-linux-gnu %s -o %t.o
# RUN: ld.lld -T %S/Inputs/alpha-gp.script %t.o -o %t
# RUN: llvm-objdump -d --no-show-raw-insn %t | FileCheck %s
# RUN: llvm-objdump -s -j .got %t | FileCheck --check-prefix=GOT %s

## gp is .got + 0x8000 = 0x120018000. .tdata has p_align 1, so x is at
## tp + 16: the high part is 0 and the low part is 16.
# CHECK:      120000000: ldah $1, 0($31)
# CHECK-NEXT: 120000004: lda $1, 16($1)
## Initial Exec: a GOT entry holding the tp offset, at .got+0.
# CHECK-NEXT: 120000008: ldq $2, -32768($29)
## General Dynamic: a module index / offset pair at .got+8.
# CHECK-NEXT: 12000000c: lda $16, -32760($29)
## Local Dynamic: the shared module index pair at .got+0x18.
# CHECK-NEXT: 120000010: lda $16, -32744($29)
## dtpoff of x is 0.
# CHECK-NEXT: 120000014: ldah $3, 0($16)
# CHECK-NEXT: 120000018: lda $3, 0($3)

## The IE entry holds 16, and the module index of the executable is 1.
# GOT:      Contents of section .got:
# GOT-NEXT: 120010000 10000000 00000000 01000000 00000000
# GOT-NEXT: 120010010 00000000 00000000 01000000 00000000
# GOT-NEXT: 120010020 00000000 00000000

.text
.globl _start
_start:
  ldah $1, x($31)       !tprelhi
  lda $1, x($1)         !tprello
  ldq $2, x($29)        !gottprel
  lda $16, x($29)       !tlsgd
  lda $16, x($29)       !tlsldm
  ldah $3, x($16)       !dtprelhi
  lda $3, x($3)         !dtprello
  ret

.section .tdata,"awT",@progbits
.globl x
x:
  .quad 0
