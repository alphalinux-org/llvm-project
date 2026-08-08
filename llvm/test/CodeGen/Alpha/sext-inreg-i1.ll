; RUN: llc -mtriple=alpha-unknown-linux-gnu -mcpu=ev6 < %s | FileCheck %s

; Sign-extending a single bit has no dedicated instruction; it lowers to a mask
; of bit 0 followed by a negate, giving 0 or -1.

; CHECK-LABEL: sexti1:
; CHECK: and $16, 1, $0
; CHECK: subq $31, $0, $0
define i64 @sexti1(i64 %x) {
  %b = trunc i64 %x to i1
  %s = sext i1 %b to i64
  ret i64 %s
}
