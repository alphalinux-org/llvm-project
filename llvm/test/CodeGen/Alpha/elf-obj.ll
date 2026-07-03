; RUN: llc -mtriple=alpha-unknown-linux-gnu -filetype=obj < %s \
; RUN:   | llvm-objdump -s -j .text - | FileCheck %s

; Direct object emission encodes instructions to the correct bytes.  The .text
; contents are addq/ret (add) followed by sll/ret (sh), and nothing here needs a
; relocation.

; CHECK: Contents of section .text:
; CHECK-NEXT: 0000 00041142 0180fa6b 2077004a 0180fa6b
define i64 @add(i64 %a, i64 %b) {
  %r = add i64 %a, %b
  ret i64 %r
}

define i64 @sh(i64 %a) {
  %r = shl i64 %a, 3
  ret i64 %r
}
