; RUN: llc -mtriple=alpha-unknown-linux-gnu -mcpu=ev6 < %s | FileCheck %s
; RUN: llc -mtriple=alpha-unknown-linux-gnu -mcpu=ev4 < %s | FileCheck %s

; Truncation keeps the low bits already in the register (free), and a 32-bit
; value is held sign-extended, so widening one is free: ldl and addl already
; produce that form.

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

; Sign extension is only the cheaper direction from i32.  Narrower widths go the
; other way, and preferring a sign extension for them costs instructions: an i8
; zero-extends with one zapnot, but sign-extends with sextb only when BWX is
; present and with an sll/sra pair otherwise.  This function is the case that
; regressed when isSExtCheaperThanZExt answered yes for every integer pair --
; it emitted `sll 56' / `sra 56' here on ev4.
; CHECK-LABEL: narrowcmp:
; CHECK:      zapnot $18, 1, $0
; CHECK-NEXT: cmpult $0, 5, $0
; CHECK-NOT:  sll
; CHECK-NOT:  sra
; CHECK:      ret
define i64 @narrowcmp(i64 %a, i64 %b, i8 %x) {
  %c = icmp ult i8 %x, 5
  %z = zext i1 %c to i64
  %r = add i64 %z, %a
  ret i64 %r
}

; A zero-extended i32 load still costs the zapnot; nothing turns it into a
; sign extension, because the value is not known non-negative.
; CHECK-LABEL: zload:
; CHECK:      ldl $0, 0($16)
; CHECK-NEXT: zapnot $0, 15, $0
; CHECK:      ret
define i64 @zload(ptr %p) {
  %v = load i32, ptr %p
  %z = zext i32 %v to i64
  ret i64 %z
}
