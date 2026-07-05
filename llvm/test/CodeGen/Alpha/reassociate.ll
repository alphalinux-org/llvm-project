; RUN: llc -mtriple=alpha-unknown-linux-gnu -mcpu=ev6 < %s | FileCheck %s

; The machine combiner reassociates chains of associative operations into a
; balanced tree so independent operations issue in parallel, shortening the
; critical path on the out-of-order 21264.

; a*b*c*d becomes (a*b)*(c*d): two independent multiplies feed the third.
; CHECK-LABEL: mulchain:
; CHECK:      mulq $16, $17, [[T0:\$[0-9]+]]
; CHECK:      mulq $18, $19, [[T1:\$[0-9]+]]
; CHECK:      mulq [[T0]], [[T1]], $0
; CHECK-NEXT: ret
define i64 @mulchain(i64 %a, i64 %b, i64 %c, i64 %d) {
  %x = mul i64 %a, %b
  %y = mul i64 %x, %c
  %z = mul i64 %y, %d
  ret i64 %z
}

; Floating-point add reassociates only under fast-math flags.
; CHECK-LABEL: fchain:
; CHECK:      addt $f16, $f17, [[F0:\$f[0-9]+]]
; CHECK:      addt $f18, $f19, [[F1:\$f[0-9]+]]
; CHECK:      addt [[F0]], [[F1]], $f0
; CHECK-NEXT: ret
define double @fchain(double %a, double %b, double %c, double %d) {
  %x = fadd reassoc nsz double %a, %b
  %y = fadd reassoc nsz double %x, %c
  %z = fadd reassoc nsz double %y, %d
  ret double %z
}

; Without fast-math flags the floating-point chain stays sequential.
; CHECK-LABEL: fchain_strict:
; CHECK:      addt $f16, $f17, $f0
; CHECK-NEXT: addt $f0, $f18, $f0
; CHECK-NEXT: addt $f0, $f19, $f0
; CHECK-NEXT: ret
define double @fchain_strict(double %a, double %b, double %c, double %d) {
  %x = fadd double %a, %b
  %y = fadd double %x, %c
  %z = fadd double %y, %d
  ret double %z
}
