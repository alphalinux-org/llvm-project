; RUN: llc -mtriple=alpha-unknown-linux-gnu -global-isel -global-isel-abort=1 < %s | FileCheck %s

; Boolean arithmetic is legalized to quadword arithmetic, so a value narrowed
; back to a boolean carries whatever the wider operations left above its low
; bit.  A branch, a conditional move and a sign extension all read the whole
; register, so the narrowing has to mask that debris away first.

; CHECK-LABEL: sext_bool:
; CHECK:      xor $17, {{\$[0-9]+}}, [[X:\$[0-9]+]]
; CHECK-NEXT: and [[X]], 1, [[A:\$[0-9]+]]
; CHECK-NEXT: subq $31, [[A]], $0
define signext i1 @sext_bool(i32 signext %a, i1 signext %b) {
  %c = icmp eq i32 %a, 0
  %d = xor i1 %b, %c
  ret i1 %d
}

; CHECK-LABEL: brcond_bool:
; CHECK:      xor $16, {{\$[0-9]+}}, [[X:\$[0-9]+]]
; CHECK-NEXT: and [[X]], 1, [[A:\$[0-9]+]]
; CHECK-NEXT: beq [[A]],
define i64 @brcond_bool(i1 signext %a, i1 signext %b) {
  %c = xor i1 %a, %b
  br i1 %c, label %t, label %f

t:
  ret i64 1

f:
  ret i64 0
}
