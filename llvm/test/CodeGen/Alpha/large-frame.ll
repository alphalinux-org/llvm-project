; RUN: llc -mtriple=alpha-unknown-linux-gnu -O2 < %s | FileCheck %s

; A stack frame larger than a 16-bit displacement is allocated (and freed) by
; building the amount in the $28 scratch and adding it to the stack pointer,
; rather than a single lda that cannot reach.

; The amounts are the point.  The frame is 5000 * 8 = 40000 bytes, so the
; prologue subtracts it as 25536 - 65536 and the epilogue adds it back as
; 65536 - 25536.
; CHECK-LABEL: big:
; CHECK:      lda $28, 25536($31)
; CHECK-NEXT: ldah $28, -1($28)
; CHECK-NEXT: addq $30, $28, $30
;
; Reaching an element too far from the stack pointer for a displacement takes
; the same two-instruction build: the last element sits at 4999 * 8 = 39992,
; written as 65536 - 25544.  This is the address arithmetic the selector emits;
; eliminateFrameIndex's own scratch-register path is exercised by @far below.
; CHECK:      ldah $0, 1($31)
; CHECK-NEXT: lda $0, -25544($0)
; CHECK:      addq $1, $0, $0
; CHECK-NEXT: ldq $0, 0($0)
;
; CHECK:      lda $28, -25536($31)
; CHECK-NEXT: ldah $28, 1($28)
; CHECK-NEXT: addq $30, $28, $30
; CHECK:      ret
define i64 @big() {
  %a = alloca [5000 x i64]
  %p0 = getelementptr [5000 x i64], ptr %a, i64 0, i64 0
  store i64 42, ptr %p0
  %p1 = getelementptr [5000 x i64], ptr %a, i64 0, i64 4999
  %v1 = load i64, ptr %p1
  ret i64 %v1
}

; A frame index that itself resolves beyond a 16-bit displacement takes
; eliminateFrameIndex's scratch-register path.  The call makes $26 callee-saved,
; and its spill slot sits above the 40000-byte array at offset 40008, which no
; displacement reaches.  The high part is materialized into $28 with ldah and
; the low part stays in the store: 65536 - 25528 = 40008.
; CHECK-LABEL: far:
; CHECK:      ldah $28, 1($30)
; CHECK-NEXT: stq $26, -25528($28)
; CHECK:      jsr $26, ($27)
; CHECK:      ldah $28, 1($30)
; CHECK-NEXT: ldq $26, -25528($28)
; CHECK:      ret
declare void @g(ptr)
define void @far() {
  %big = alloca [5000 x i64]
  call void @g(ptr %big)
  ret void
}
