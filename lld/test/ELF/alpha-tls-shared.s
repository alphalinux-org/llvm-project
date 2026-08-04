# REQUIRES: alpha
## In a shared object no TLS model is optimized away, so each of general
## dynamic, local dynamic and initial exec gets its own GOT slots and dynamic
## relocations.
# RUN: llvm-mc -filetype=obj -triple=alpha-unknown-linux-gnu %s -o %t.o
# RUN: ld.lld -shared -T %S/Inputs/alpha-shared.script %t.o -o %t.so
# RUN: llvm-readobj -r %t.so | FileCheck %s
# RUN: llvm-objdump -d --no-show-raw-insn %t.so | FileCheck --check-prefix=DIS %s

## The script puts .got at 0x20000. Entries are allocated in reference order:
## the general dynamic pair for x, the local dynamic module index pair, then
## x's tp offset.
# CHECK:      Section ({{.*}}) .rela.dyn {
# CHECK-NEXT:   0x20010 R_ALPHA_DTPMOD64 - 0x0
# CHECK-NEXT:   0x20000 R_ALPHA_DTPMOD64 x 0x0
# CHECK-NEXT:   0x20008 R_ALPHA_DTPREL64 x 0x0
# CHECK-NEXT:   0x20020 R_ALPHA_TPREL64 x 0x0
# CHECK-NEXT: }

## tlsgd -> .got+0, tlsldm -> .got+0x10, gottprel -> .got+0x20.
# DIS:      lda $16, -32768($29)
# DIS-NEXT: lda $16, -32752($29)
## y is at offset 8 in the TLS block, so dtpoff is 0 * 0x10000 + 8.
# DIS-NEXT: ldah $3, 0($16)
# DIS-NEXT: lda $3, 8($3)
# DIS-NEXT: ldq $2, -32736($29)

.text
.globl _start
_start:
  lda $16, x($29)       !tlsgd
  lda $16, y($29)       !tlsldm
  ldah $3, y($16)       !dtprelhi
  lda $3, y($3)         !dtprello
  ldq $2, x($29)        !gottprel
  ret

.section .tdata,"awT",@progbits
.globl x
x:
  .quad 0
y:
  .quad 0
