# REQUIRES: alpha
## A !samegp branch skips the callee's gp-load prologue, so the callee has to
## say in st_other whether it has one: STO_ALPHA_NOPV (no adjustment) or
## STO_ALPHA_STD_GPLOAD (skip two instructions).  A symbol marked neither --
## hand-written assembly missing .prologue or .usepv -- must be diagnosed, not
## silently treated as NOPV, which would branch into a function that does set
## up its own gp while skipping the instructions that do it.
##
## GNU ld reports "!samegp reloc against symbol without .prologue: plain".
# RUN: llvm-mc -filetype=obj -triple=alpha-unknown-linux-gnu %s -o %t.o
# RUN: not ld.lld -T %S/Inputs/alpha-gp.script %t.o -o /dev/null 2>&1 \
# RUN:   | FileCheck %s

# CHECK: error: {{.*}}!samegp reloc against symbol without .prologue: plain

.text
.globl _start
.ent _start
_start:
  br $26, plain         !samegp
  ret
.end _start

## No .prologue, so st_other stays 0.
.globl plain
plain:
  ret
