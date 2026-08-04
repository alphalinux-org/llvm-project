# REQUIRES: alpha
# RUN: llvm-mc -filetype=obj -triple=alpha-unknown-linux-gnu %s -o %t.o
# RUN: ld.lld -m elf64alpha %t.o -o %t
# RUN: llvm-readobj --file-headers %t | FileCheck %s
# RUN: ld.lld %t.o -o %t2
# RUN: llvm-readobj --file-headers %t2 | FileCheck %s

## OUTPUT_FORMAT selects the same target.
# RUN: echo 'OUTPUT_FORMAT(elf64-alpha)' > %t.script
# RUN: ld.lld %t.script %t.o -o %t3
# RUN: llvm-readobj --file-headers %t3 | FileCheck %s

# CHECK:      Class: 64-bit
# CHECK-NEXT: DataEncoding: LittleEndian
# CHECK:      Machine: EM_ALPHA (0x9026)

.globl _start
_start:
  ret
