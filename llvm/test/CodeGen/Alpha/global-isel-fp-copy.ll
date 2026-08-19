; RUN: llc -mtriple=alpha-unknown-linux-gnu -global-isel -global-isel-abort=1 < %s | FileCheck %s

; A float argument that arrives in an integer register -- which is where the
; bits of a float loaded as a longword sit -- has to reach $f16 through memory:
; there is no instruction that moves a value between the two register banks, and
; the store/load pair also converts the S_floating format on the way in.

declare i32 @bar(float)

; CHECK-LABEL: foo:
; CHECK:      stl {{\$[0-9]+}}, [[OFF:[0-9]+]]($30)
; CHECK-NEXT: lds $f16, [[OFF]]($30)
; CHECK:      jsr $26, ($27)
define void @foo(ptr %p, ptr %q) {
  %f = load float, ptr %p
  %r = call i32 @bar(float %f)
  store i32 %r, ptr %q
  ret void
}
