; RUN: llc -mtriple=alpha-unknown-linux-gnu -global-isel -global-isel-abort=0 < %s | FileCheck %s

; What GlobalISel deliberately does not handle, and hands to the SelectionDAG
; path whole.  Each of these needs machinery the selector has no counterpart
; for, so the legalizer marks it unsupported rather than calling it legal: an
; unsupported opcode is a clean fall back, where a legal one reaches a selector
; with nothing to select and stops the compilation.
;
; The output below is the SelectionDAG lowering, which is what makes the point:
; these compile correctly, just not through GlobalISel.

; A jump table dispatch is the gp-relative sequence LowerBR_JT builds.
; CHECK-LABEL: switch_jt:
; CHECK:       .LJTI0_0
; CHECK:       jmp $31, ($0), 0
define i64 @switch_jt(i64 %x) {
entry:
  switch i64 %x, label %d [ i64 0, label %a
                            i64 1, label %b
                            i64 2, label %c
                            i64 3, label %a
                            i64 4, label %b ]
a:
  ret i64 10
b:
  ret i64 20
c:
  ret i64 30
d:
  ret i64 40
}

; The address of a block is formed gp-relative like a global's.
@ba = global ptr null
; CHECK-LABEL: blockaddr:
; CHECK:       gprelhigh
define void @blockaddr() {
  store ptr blockaddress(@blockaddr, %here), ptr @ba
  br label %here
here:
  ret void
}

; A read-modify-write becomes an ldq_l/stq_c retry loop, which the SelectionDAG
; path builds with a custom inserter.
; CHECK-LABEL: rmw:
; CHECK:       ldq_l
; CHECK:       stq_c
define i64 @rmw(ptr %p, i64 %v) {
  %a = atomicrmw add ptr %p, i64 %v seq_cst
  ret i64 %a
}
