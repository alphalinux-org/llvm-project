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

; There is no divide instruction; the SelectionDAG path calls __divq, which
; takes its arguments in $24/$25 and returns in $27.
; CHECK-LABEL: sdiv:
; CHECK:       __divq
define i64 @sdiv(i64 %a, i64 %b) {
  %r = sdiv i64 %a, %b
  ret i64 %r
}

; CHECK-LABEL: urem:
; CHECK:       __remqu
define i64 @urem(i64 %a, i64 %b) {
  %r = urem i64 %a, %b
  ret i64 %r
}

; umulh is an instruction and a signed high multiply is not: the overflow
; intrinsic lowers through G_SMULH, which the SelectionDAG path open-codes.
; CHECK-LABEL: smulo:
; CHECK:       umulh
define i1 @smulo(i64 %a, i64 %b) {
  %r = call {i64, i1} @llvm.smul.with.overflow.i64(i64 %a, i64 %b)
  %o = extractvalue {i64, i1} %r, 1
  ret i1 %o
}

; Only the 64-bit bitconvert has a pattern.  The 32-bit pair reaches
; MOVi2f_S/MOVf2i_S through custom lowering GlobalISel does not produce.
; CHECK-LABEL: bitcast_i32_f32:
; CHECK:       sts
define i32 @bitcast_i32_f32(float %x) {
  %r = bitcast float %x to i32
  ret i32 %r
}
