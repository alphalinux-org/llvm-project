; RUN: llc -mtriple=alpha-unknown-linux-gnu -mcpu=ev6 < %s | FileCheck %s

; Truncation keeps the low bits already in the register (free), and a value
; narrower than a register is held sign-extended, so the compiler prefers a
; sign extension (ldl/addl) over a zero extension (which needs a zapnot).

; A sign-extended i32 load is a single ldl, no zapnot.
; CHECK-LABEL: sload:
; CHECK:      ldl $0, 0($16)
; CHECK-NOT:  zapnot
; CHECK:      ret
define i64 @sload(ptr %p) {
  %v = load i32, ptr %p
  %s = sext i32 %v to i64
  ret i64 %s
}

; Truncating the sum to i32 needs no separate instruction.
; CHECK-LABEL: truncadd:
; CHECK:      addq $16, $17, $0
; CHECK-NEXT: ret
define i32 @truncadd(i64 %a, i64 %b) {
  %t = add i64 %a, %b
  %r = trunc i64 %t to i32
  ret i32 %r
}

; Where the extension kind is the compiler's own choice, isSExtCheaperThanZExt
; picks the sign extension: a zext the front end has marked nneg becomes a sign
; extension, so the add is an addl -- which already leaves the value in its
; canonical sign-extended form -- rather than an addq and a zapnot.
; CHECK-LABEL: zext_nneg:
; CHECK:      addl $16, $17, $0
; CHECK-NOT:  zapnot
; CHECK:      ret
define i64 @zext_nneg(i32 %a, i32 %b) {
  %s = add i32 %a, %b
  %z = zext nneg i32 %s to i64
  ret i64 %z
}
