; RUN: llc < %s -mtriple=i686-- -mattr=-sse2 | FileCheck %s

; Without SSE2 a double constant goes in the constant pool as a float when the
; float has the same value, and stays a double when it does not.  Whether it
; may be shrunk is a question about the value, not only about the type, which
; is why ShouldShrinkFPConstant is asked about the value the pool would hold.

define double @representable() nounwind {
; CHECK-LABEL: representable:
; CHECK: flds
  ret double 2.5
}

define double @not_representable() nounwind {
; CHECK-LABEL: not_representable:
; CHECK: fldl
  ret double 0x3FB999999999999A
}
