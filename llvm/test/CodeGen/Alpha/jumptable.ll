; RUN: llc -mtriple=alpha-unknown-linux-gnu < %s | FileCheck %s

; A dense switch becomes a jump table: bounds-check the index, form the table
; address GP-relative, load the absolute target and branch to it.

declare void @a()
declare void @b()
declare void @c()
declare void @d()
declare void @e()

; CHECK-LABEL: f:
; CHECK:       cmpult {{.*}}, $16,
; CHECK:       ldah $0, .LJTI0_0($29){{.*}}!gprelhigh
; CHECK:       lda $0, .LJTI0_0($0){{.*}}!gprellow
; CHECK:       s8addq $16, $0, $0
; CHECK:       ldq $0, 0($0)
; CHECK:       jmp $31, ($0), 0

; The table is emitted as absolute 64-bit block addresses.
; CHECK:       .LJTI0_0:
; CHECK-NEXT:  .quad .LBB0_
define void @f(i64 %x) {
entry:
  switch i64 %x, label %ret [
    i64 0, label %ca
    i64 1, label %cb
    i64 2, label %cc
    i64 3, label %cd
    i64 4, label %ce
  ]
ca:
  call void @a()
  br label %ret
cb:
  call void @b()
  br label %ret
cc:
  call void @c()
  br label %ret
cd:
  call void @d()
  br label %ret
ce:
  call void @e()
  br label %ret
ret:
  ret void
}


; The out-of-range index takes the default edge: the bounds check is a cmpult
; against the table size and a branch around the dispatch.  Nothing covered
; either the check or the default block, so a table built without a guard would
; have passed.
; CHECK-LABEL: jt_default:
; CHECK:       cmpult
; CHECK:       bne
; CHECK:       jmp $31, ($0), 0
; CHECK:       lda $0, -1($31)
define i64 @jt_default(i64 %x) {
entry:
  switch i64 %x, label %def [ i64 0, label %a
                              i64 1, label %b
                              i64 2, label %c
                              i64 3, label %d
                              i64 4, label %e ]
a: ret i64 10
b: ret i64 20
c: ret i64 30
d: ret i64 40
e: ret i64 50
def: ret i64 -1
}
