; RUN: llc -mtriple=alpha-unknown-linux-gnu -mcpu=ev6 < %s | FileCheck %s

; CHECK-LABEL: sexti1:
; CHECK: and $16, 1, $0
; CHECK: subq $31, $0, $0
define i64 @sexti1(i64 %x) {
  %b = trunc i64 %x to i1
  %s = sext i1 %b to i64
  ret i64 %s
}
