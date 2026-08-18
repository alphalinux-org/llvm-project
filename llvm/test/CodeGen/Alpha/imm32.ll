; RUN: llc -mtriple=alpha-unknown-linux-gnu < %s | FileCheck %s

; 100000 = 2 * 65536 - 31072
; CHECK-LABEL: pos:
; CHECK:       ldah $0, 2($31)
; CHECK-NEXT:  lda $0, -31072($0)
; CHECK-NEXT:  ret
define i64 @pos() {
  ret i64 100000
}

; CHECK-LABEL: neg:
; CHECK:       ldah $0, -2($31)
; CHECK-NEXT:  lda $0, 31072($0)
; CHECK-NEXT:  ret
define i64 @neg() {
  ret i64 -100000
}

; 65536 has a zero low half, so the lda is omitted.
; CHECK-LABEL: shifted:
; CHECK:       ldah $0, 1($31)
; CHECK-NEXT:  ret
define i64 @shifted() {
  ret i64 65536
}

; 0x40000000 fits in a single ldah.
; CHECK-LABEL: high_bit:
; CHECK:       ldah $0, 16384($31)
; CHECK-NEXT:  ret
define i64 @high_bit() {
  ret i64 1073741824
}

; INT32_MIN = 0x80000000 sign-extends from a single ldah of -32768.
; CHECK-LABEL: int32_min:
; CHECK:       ldah $0, -32768($31)
; CHECK-NEXT:  ret
define i64 @int32_min() {
  ret i64 -2147483648
}

; 0x7ffffffb needs a high half of 0x8000, which does not fit ldah's signed
; field, so it is built from two ldah of 0x4000 plus the low half.
; CHECK-LABEL: near_int32_max:
; CHECK:       ldah $0, 16384($31)
; CHECK-NEXT:  ldah $0, 16384($0)
; CHECK-NEXT:  lda $0, -5($0)
; CHECK-NEXT:  ret
define i64 @near_int32_max() {
  ret i64 2147483643
}

; INT32_MAX = 0x7fffffff, likewise.
; CHECK-LABEL: int32_max:
; CHECK:       ldah $0, 16384($31)
; CHECK-NEXT:  ldah $0, 16384($0)
; CHECK-NEXT:  lda $0, -1($0)
; CHECK-NEXT:  ret
define i64 @int32_max() {
  ret i64 2147483647
}
