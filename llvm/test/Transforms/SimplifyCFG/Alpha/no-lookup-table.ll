; RUN: opt -mtriple=alpha-unknown-linux-gnu -passes='simplifycfg<switch-to-lookup>' \
; RUN:   -S < %s | FileCheck %s

; AlphaTTIImpl::shouldBuildLookupTables is false: reaching a table in .rodata
; costs a gp-relative ldah/lda pair before the load, and the table competes for
; the 64KB window around the global pointer that is already the scarce resource
; here.  So a switch that another target would turn into a lookup table stays a
; chain of compares.

; CHECK-LABEL: @f(
; CHECK-NOT: switch.table
define i32 @f(i32 %x) {
entry:
  switch i32 %x, label %def [
    i32 0, label %a
    i32 1, label %b
    i32 2, label %c
    i32 3, label %d
    i32 4, label %e
    i32 5, label %g
  ]

a: br label %ret
b: br label %ret
c: br label %ret
d: br label %ret
e: br label %ret
g: br label %ret
def: br label %ret

ret:
  %p = phi i32 [ 17, %a ], [ 3, %b ], [ 91, %c ], [ 12, %d ], [ 55, %e ], [ 8, %g ], [ 0, %def ]
  ret i32 %p
}
