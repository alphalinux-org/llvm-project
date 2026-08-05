; RUN: llc -mtriple=alpha-unknown-linux-gnu -global-isel -global-isel-abort=1 < %s | FileCheck %s

; Every incoming value of a phi shares the bank of the result, and the decision
; has to account for both ends: a phi merging a value that came out of a
; floating operation belongs in the floating bank even if nothing downstream
; says so, or the two would disagree and there would be no way to copy between
; them.

; The phi and everything reaching it stay in the floating bank, so the result
; is already in $f0 and no copy through memory is needed.
; CHECK-LABEL: merge_fp:
; CHECK-NOT:   stt {{.*}}($30)
; CHECK:       ret
define double @merge_fp(i1 %c, double %a, double %b) {
entry:
  br i1 %c, label %t, label %f
t:
  %x = fadd double %a, %b
  br label %join
f:
  %y = fmul double %a, %b
  br label %join
join:
  %r = phi double [ %x, %t ], [ %y, %f ]
  ret double %r
}

; The result is only ever stored, so nothing downstream marks the phi as
; floating; the incoming values are what decide it.
; Here nothing downstream is floating -- the value is only stored -- so the
; incoming values are what decide it.  Getting that wrong puts the phi in one
; bank and the store in the other, and the value reaches the store through the
; stack: stt to a frame slot, ldq back, stq out.  A single stt is the whole
; point.
; CHECK-LABEL: merge_fp_stored:
; CHECK:       stt $f18, 0($19)
; CHECK-NOT:   ldq
; CHECK:       ret
define void @merge_fp_stored(i1 %c, double %a, double %b, ptr %p) {
entry:
  br i1 %c, label %t, label %f
t:
  %x = fadd double %a, %b
  br label %join
f:
  br label %join
join:
  %r = phi double [ %x, %t ], [ %b, %f ]
  store double %r, ptr %p
  ret void
}
