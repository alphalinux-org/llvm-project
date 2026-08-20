# REQUIRES: alpha
## Alpha binds eagerly and emits no PLT: a call loads the callee address from
## the ordinary GOT, so a preemptible symbol just needs R_ALPHA_GLOB_DAT there.
# RUN: llvm-mc -filetype=obj -triple=alpha-unknown-linux-gnu %s -o %t.o
# RUN: ld.lld -shared -T %S/Inputs/alpha-shared.script %t.o -o %t.so
# RUN: llvm-readelf -S %t.so | FileCheck --check-prefix=SEC %s
# RUN: llvm-readobj -r %t.so | FileCheck %s
# RUN: llvm-objdump -s -j .got %t.so | FileCheck --check-prefix=GOT %s

## Alpha uses RELA, and there is no .plt or .got.plt.
# SEC:     .rela.dyn RELA
# SEC-NOT: .plt

## The script puts .got at 0x20000 and .data at 0x30000.
# CHECK:      Section ({{.*}}) .rela.dyn {
# CHECK-NEXT:   0x20008 R_ALPHA_RELATIVE - 0x30000
# CHECK-NEXT:   0x20000 R_ALPHA_GLOB_DAT ext 0x0
# CHECK-NEXT:   0x20010 R_ALPHA_GLOB_DAT extvar 0x0
# CHECK-NEXT:   0x30008 R_ALPHA_REFQUAD expvar 0x0
# CHECK-NEXT: }

## glibc's R_ALPHA_RELATIVE handler adds the load bias to the value already in
## place and ignores r_addend, so the addend must also be written to .got.
# GOT:      Contents of section .got:
# GOT-NEXT: 20000 00000000 00000000 00000300 00000000
# GOT-NEXT: 20010 00000000 00000000

.text
.globl fn
fn:
  ldq $1, ext($29)      !literal
  ldq $2, loc($29)      !literal
  ldq $3, extvar($29)   !literal
  ret

.data
loc:
  .quad 0
.globl expvar
expvar:
  .quad expvar
