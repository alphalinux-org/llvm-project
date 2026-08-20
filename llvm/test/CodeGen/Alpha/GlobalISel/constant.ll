; RUN: llc -mtriple=alpha-unknown-linux-gnu -global-isel -global-isel-abort=1 < %s | FileCheck %s

; lda carries a signed 16-bit displacement and ldah the same shifted left 16, so
; a constant that fits in 32 bits is built from a pair of them; anything wider
; goes in the constant pool.  A pattern covers the 16-bit case already.

; CHECK-LABEL: small:
; CHECK: lda $0, 1234($31)
define i64 @small() {
  ret i64 1234
}

; CHECK-LABEL: negative_small:
; CHECK: lda $0, -1234($31)
define i64 @negative_small() {
  ret i64 -1234
}

; 305419896 is 0x12345678: high half 0x1234 = 4660, low half 0x5678 = 22136.
; Both halves are asserted, so a split that is off by any amount fails.
; CHECK-LABEL: wide32:
; CHECK:      ldah $0, 4660($31)
; CHECK-NEXT: lda $0, 22136($0)
define i64 @wide32() {
  ret i64 305419896
}

; A high half that does not fit ldah's signed field takes two of them, each
; carrying half of it: 0x8000 is written as 16384 twice, and the low half is
; then -1.
; CHECK-LABEL: high_bit:
; CHECK:      ldah $0, 16384($31)
; CHECK-NEXT: ldah $0, 16384($0)
; CHECK-NEXT: lda $0, -1($0)
define i64 @high_bit() {
  ret i64 2147483647
}

; Wider than 32 bits: load it from the constant pool, addressed from the global
; pointer.
; CHECK-LABEL: wide64:
; CHECK: ldah $[[R:[0-9]+]], {{.*}}!gprelhigh
; CHECK: ldq $0, {{.*}}!gprellow
define i64 @wide64() {
  ret i64 1234605616436508552
}
