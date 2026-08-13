; RUN: llc -mtriple=alpha-unknown-linux-gnu -mattr=+ieee < %s | FileCheck %s

; A branch on a floating-point comparison branches on the comparison result
; itself, which cmpt leaves as 2.0 or 0.0.  Materializing a 0/1 boolean to test
; in an integer register would cost an ftoit and a shift where the FIX extension
; is available, and a store/load round trip through the stack where it is not.

declare void @sink()

; CHECK-LABEL: olt:
; CHECK:       cmptlt/su $f16, $f17, $f0
; CHECK-NEXT:  fbeq $f0, .LBB0_2
; CHECK-NOT:   ftoit
; CHECK-NOT:   stt
define void @olt(double %a, double %b) {
  %c = fcmp olt double %a, %b
  br i1 %c, label %t, label %e
t:
  call void @sink()
  ret void
e:
  ret void
}

; Greater-than swaps the operands, since only cmptlt and cmptle exist.
; CHECK-LABEL: ogt:
; CHECK:       cmptlt/su $f17, $f16, $f0
; CHECK-NEXT:  fbeq $f0, .LBB1_2
define void @ogt(double %a, double %b) {
  %c = fcmp ogt double %a, %b
  br i1 %c, label %t, label %e
t:
  call void @sink()
  ret void
e:
  ret void
}

; An inequality inverts to cmpteq, and the branch takes the opposite sense
; rather than flipping a materialized boolean with an xor.
; CHECK-LABEL: une:
; CHECK:       cmpteq/su $f16, $f17, $f0
; CHECK-NEXT:  fbne $f0, .LBB2_2
; CHECK-NOT:   xor
define void @une(double %a, double %b) {
  %c = fcmp une double %a, %b
  br i1 %c, label %t, label %e
t:
  call void @sink()
  ret void
e:
  ret void
}

; f32 compares in the same T_floating instructions.
; CHECK-LABEL: oeq_f32:
; CHECK:       cmpteq/su $f16, $f17, $f0
; CHECK-NEXT:  fbeq $f0, .LBB3_2
define void @oeq_f32(float %a, float %b) {
  %c = fcmp oeq float %a, %b
  br i1 %c, label %t, label %e
t:
  call void @sink()
  ret void
e:
  ret void
}

; The comparison result is still needed as a value here, so the boolean gets
; materialized as well -- but the branch itself stays in the FP unit.
; CHECK-LABEL: also_used:
; CHECK:       cmptlt/su $f16, $f17, $f[[C:[0-9]+]]
; CHECK-NEXT:  fbeq $f[[C]],
define i64 @also_used(double %a, double %b) {
  %c = fcmp olt double %a, %b
  br i1 %c, label %t, label %e
t:
  call void @sink()
  br label %e
e:
  %z = zext i1 %c to i64
  ret i64 %z
}
