# REQUIRES: alpha
## R_ALPHA_GOTDTPREL puts a symbol's module-relative offset in the GOT, which
## local-dynamic uses when the offset does not fit a 16-bit displacement.
# RUN: llvm-mc -filetype=obj -triple=alpha-unknown-linux-gnu %s -o %t.o

## Linked into an executable the offsets are known, so they are constants.
# RUN: ld.lld -T %S/Inputs/alpha-gp.script %t.o -o %t
# RUN: llvm-objdump -d --no-show-raw-insn %t | FileCheck %s
# RUN: llvm-objdump -s -j .got %t | FileCheck --check-prefix=GOT %s

## x is at offset 0 in the TLS block and y at offset 8.
# CHECK:      120000000: ldq $1, -32768($29)
# CHECK-NEXT: 120000004: ldq $2, -32760($29)
# GOT:      Contents of section .got:
# GOT-NEXT: 120010000 00000000 00000000 08000000 00000000

## In a shared object a preemptible symbol's offset is only known at run time,
## so its entry gets R_ALPHA_DTPREL64, as bfd does. y is local and stays a
## constant.
# RUN: ld.lld -shared -T %S/Inputs/alpha-shared.script %t.o -o %t.so
# RUN: llvm-readobj -r %t.so | FileCheck --check-prefix=DYN %s
# DYN:      Section ({{.*}}) .rela.dyn {
# DYN-NEXT:   0x20000 R_ALPHA_DTPREL64 x 0x0
# DYN-NEXT: }

.text
.globl _start
_start:
  ldq $1, x($29)        !gotdtprel
  ldq $2, y($29)        !gotdtprel
  ret

.section .tdata,"awT",@progbits
.globl x
x:
  .quad 0
y:
  .quad 0
