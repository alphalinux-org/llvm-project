; RUN: llc -mtriple=alpha-unknown-linux-gnu < %s | FileCheck %s

; The DWARF2 unwinder enters a landing pad with the exception pointer in $a0
; ($16) and the type selector in $a1 ($17), matching libgcc's
; EH_RETURN_DATA_REGNO(0/1) for Alpha.

declare void @may_throw()
declare i32 @__gxx_personality_v0(...)

; The landing pad reads the selector out of $17.  With the wrong registers
; nominated the pad would read $0 or a spill slot instead, which is what this
; checks -- the label alone said nothing.
; CHECK-LABEL: caught:
; CHECK:       addl $17, $31, $0
define i64 @caught() personality ptr @__gxx_personality_v0 {
entry:
  invoke void @may_throw() to label %done unwind label %lpad

lpad:
  %e = landingpad { ptr, i32 } cleanup
  %sel = extractvalue { ptr, i32 } %e, 1
  %r = sext i32 %sel to i64
  ret i64 %r

done:
  ret i64 0
}

; The exception pointer comes back in $16 and the selector in $17, so a pad
; that returns the pointer reads $16.
; CHECK-LABEL: caught_ptr:
; CHECK:       bis $31, $16, $0
define ptr @caught_ptr() personality ptr @__gxx_personality_v0 {
entry:
  invoke void @may_throw() to label %done unwind label %lpad

lpad:
  %e = landingpad { ptr, i32 } cleanup
  %p = extractvalue { ptr, i32 } %e, 0
  ret ptr %p

done:
  ret ptr null
}
