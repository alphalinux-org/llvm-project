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

; When the two ends really do disagree -- one incoming value is in the other
; bank -- the phi cannot simply take it, because phi elimination turns the phi
; into a copy per edge and there is no copy between the two register files.
; The move goes in at the end of the block the value comes from, so the copy
; the phi becomes finds it already in the right bank.

; The integer argument is moved into the floating bank in the block it reaches
; the phi from, not in the join block.
; CHECK-LABEL: phi_to_fp:
; CHECK:       blb{{[sc]}} $16,
; CHECK:       stq $18, {{[0-9]+}}($30)
; CHECK-NEXT:  ldt $f{{[0-9]+}}, {{[0-9]+}}($30)
; CHECK:       addt
define double @phi_to_fp(i1 %c, double %a, i64 %x) {
entry:
  br i1 %c, label %t, label %f
t:
  br label %j
f:
  %b = bitcast i64 %x to double
  br label %j
j:
  %p = phi double [ %a, %t ], [ %b, %f ]
  %r = fadd double %p, %p
  ret double %r
}

; The other direction.  The loaded value is used by an fadd, so it is in the
; floating bank, but nothing that reaches the phi is defined by a floating
; operation, so the phi itself is an integer one -- and the value has to come
; back across.
; CHECK-LABEL: phi_to_gpr:
; CHECK:       blb{{[sc]}} $16,
; CHECK:       stt $f{{[0-9]+}}, {{[0-9]+}}($30)
; CHECK-NEXT:  ldq {{\$[0-9]+}}, {{[0-9]+}}($30)
; CHECK:       addq
define i64 @phi_to_gpr(i1 %c, i64 %a, ptr %p, ptr %q) {
entry:
  %l = load double, ptr %p
  %d = fadd double %l, %l
  store double %d, ptr %q
  br i1 %c, label %t, label %f
t:
  br label %j
f:
  %b = bitcast double %l to i64
  br label %j
j:
  %ph = phi i64 [ %a, %t ], [ %b, %f ]
  %r = add i64 %ph, 1
  ret i64 %r
}

; The bank walk asks "is any value reaching this phi a floating one", and that
; question recurses through phis.  Two phis in a loop that name each other
; reach themselves, so the walk has to be guarded or it does not terminate.
; Nothing here is floating and the whole loop stays in integer registers.
; CHECK-LABEL: two_phi:
; CHECK-NOT:   stt
; CHECK-NOT:   ldt
; CHECK:       stq {{\$[0-9]+}}, 0($16)
define void @two_phi(ptr %p, i64 %n) {
entry:
  br label %loop
loop:
  %a = phi i64 [ 0, %entry ], [ %b, %loop ]
  %b = phi i64 [ 1, %entry ], [ %a, %loop ]
  %i = phi i64 [ 0, %entry ], [ %i1, %loop ]
  store i64 %a, ptr %p
  %i1 = add i64 %i, 1
  %c = icmp slt i64 %i1, %n
  br i1 %c, label %loop, label %out
out:
  ret void
}
