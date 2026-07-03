; RUN: llc -mtriple=alpha-unknown-linux-gnu < %s | FileCheck %s

; CHECK-LABEL: selcmp:
; CHECK:       bis $31, $19, $0
; CHECK:       cmplt $16, $17, $1
; CHECK:       cmovne $1, $18, $0
; CHECK:       ret
define i64 @selcmp(i64 %a, i64 %b, i64 %t, i64 %f) {
  %c = icmp slt i64 %a, %b
  %r = select i1 %c, i64 %t, i64 %f
  ret i64 %r
}

; A condition narrowed from a wider value is masked to its low bit first: cmovne
; reads the whole register, so any bit left above bit 0 would take the true arm
; for a false condition.  The mask is the point of the case, so it is checked.
; CHECK-LABEL: seltrunc:
; CHECK:       and $16, {{.*}}, [[C:\$[0-9]+]]
; CHECK:       cmovne [[C]],
; CHECK:       ret
define i64 @seltrunc(i64 %c, i64 %t, i64 %f) {
  %b = trunc i64 %c to i1
  %r = select i1 %b, i64 %t, i64 %f
  ret i64 %r
}
