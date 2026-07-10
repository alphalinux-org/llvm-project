; RUN: llc -mtriple=alpha-unknown-linux-gnu < %s | FileCheck %s
; The table is gp-relative either way, so position-independent code needs no
; second form of it -- which is worth stating, since nothing exercised -fPIC
; here and a target that did need one would have shipped without noticing.
; RUN: llc -mtriple=alpha-unknown-linux-gnu -relocation-model=pic < %s \
; RUN:   | FileCheck %s
; RUN: llc -mtriple=alpha-unknown-linux-gnu -filetype=obj < %s \
; RUN:   | llvm-readelf -r - | FileCheck %s --check-prefix=RELOC
; Assembling the printed output has to reach the same relocation, which is what
; -S, -save-temps and -fno-integrated-as all depend on.
; RUN: llc -mtriple=alpha-unknown-linux-gnu < %s \
; RUN:   | llvm-mc -triple=alpha-unknown-linux-gnu -filetype=obj - \
; RUN:   | llvm-readelf -r - | FileCheck %s --check-prefix=RELOC

; A dense switch becomes a jump table: bounds-check the index, form the table
; address GP-relative, load a GP-relative 32-bit offset for the target, add the
; global pointer back to get the absolute address and branch to it.

declare void @a()
declare void @b()
declare void @c()
declare void @d()
declare void @e()

; CHECK-LABEL: f:
; CHECK:       cmpult {{.*}}, $16,
; CHECK:       ldah $0, .LJTI0_0($29){{.*}}!gprelhigh
; CHECK:       lda $0, .LJTI0_0($0){{.*}}!gprellow
; CHECK:       s4addq $16, $0, $0
; CHECK:       ldl $0, 0($0)
; CHECK:       addq $29, $0, $0
; CHECK:       jmp $31, ($0), 0

; The table holds one 32-bit GP-relative entry per target.  .gprel32 is how gcc
; and GNU as spell it; a bare .long would reassemble to R_ALPHA_REFLONG and put
; an absolute address where the dispatch sequence adds $29.
; CHECK:       .LJTI0_0:
; CHECK-NEXT:  .gprel32 .LBB0_

; Each entry is resolved GP-relative, so it fits in 32 bits regardless of where
; the text is loaded.
; RELOC: Relocation section '.rela.rodata'
; RELOC: R_ALPHA_GPREL32
; RELOC: R_ALPHA_GPREL32
; RELOC: R_ALPHA_GPREL32
; RELOC: R_ALPHA_GPREL32
; RELOC: R_ALPHA_GPREL32
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
