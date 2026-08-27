# REQUIRES: x86
## .gnu.linkonce.<kind>.<name> is the pre-COMDAT one-only mechanism: keep the
## first section with a given name and discard every later one. Still emitted by
## glibc's Alpha division helpers, among others.

# RUN: rm -rf %t && split-file %s %t && cd %t
# RUN: llvm-mc -filetype=obj -triple=x86_64 a.s -o a.o
# RUN: llvm-mc -filetype=obj -triple=x86_64 b.s -o b.o
# RUN: llvm-mc -filetype=obj -triple=x86_64 main.s -o main.o
# RUN: ld.lld main.o a.o b.o -o out
# RUN: llvm-objdump -d --no-show-raw-insn out | FileCheck %s

## The second definition is dropped rather than reported as a duplicate, and
## both callers reach the copy that was kept.
# CHECK:      <_start>:
# CHECK-NEXT:   callq {{.*}} <dup>
# CHECK:      <dup>:
# CHECK-NEXT:   movl $0x1, %eax
# CHECK-NOT:  <dup>:

## Archive members behave the same way, which is the shape the glibc failure has.
# RUN: llvm-ar rc lib.a a.o b.o
# RUN: ld.lld main.o lib.a -o out2
# RUN: llvm-objdump -d --no-show-raw-insn out2 | FileCheck %s

## Ordering decides which copy is kept, not the contents.
# RUN: ld.lld main.o b.o a.o -o out3
# RUN: llvm-objdump -d --no-show-raw-insn out3 | FileCheck %s --check-prefix=REV
# REV:      <dup>:
# REV-NEXT:   movl $0x2, %eax

## A member of a section group is not a linkonce section, whatever it is
## called: the group already says which copies go together. Discarding one
## member by name would split the group and leave the rest of it referring to a
## section that is gone -- here group h keeps .text.h, whose call to the local f
## it no longer has. GNU ld links this.
# RUN: llvm-mc -filetype=obj -triple=x86_64 gm1.s -o gm1.o
# RUN: llvm-mc -filetype=obj -triple=x86_64 gm2.s -o gm2.o
# RUN: llvm-mc -filetype=obj -triple=x86_64 main2.s -o main2.o
# RUN: ld.lld main2.o gm1.o gm2.o -o out4
# RUN: llvm-objdump -d --no-show-raw-insn out4 | FileCheck %s --check-prefix=GRP
## Both groups survive whole: two copies of f, one per group.
# GRP:      <f>:
# GRP-NEXT:   movl $0x1, %eax
# GRP:      <f>:
# GRP-NEXT:   movl $0x2, %eax

#--- a.s
.section .gnu.linkonce.t.dup,"ax",@progbits
.globl dup
dup:
  movl $1, %eax
  retq

#--- b.s
.section .gnu.linkonce.t.dup,"ax",@progbits
.globl dup
dup:
  movl $2, %eax
  retq

#--- main.s
.globl _start
_start:
  callq dup
  retq

#--- gm1.s
.section .gnu.linkonce.t.f,"axG",@progbits,g,comdat
f:
  movl $1, %eax
  retq
.section .text.g,"axG",@progbits,g,comdat
.globl g
g:
  jmp f

#--- gm2.s
.section .gnu.linkonce.t.f,"axG",@progbits,h,comdat
f:
  movl $2, %eax
  retq
.section .text.h,"axG",@progbits,h,comdat
.globl h
h:
  jmp f

#--- main2.s
.globl _start
_start:
  callq g
  callq h
  retq
