; RUN: llc -mtriple=alpha-unknown-linux-gnu -global-isel -global-isel-abort=1 \
; RUN:   < %s | FileCheck %s

; An and whose mask keeps or clears whole bytes is a zapnot: the byte pattern
; goes in the literal field, so nothing has to build the mask.  Selecting the
; register-register and instead cost a materialized constant, and for
; 0xffffffff that is a constant-pool entry reached through the gp.

; CHECK-LABEL: mask16:
; CHECK:      zapnot $16, 3, $0
; CHECK-NEXT: ret
define i64 @mask16(i64 %x) {
  %r = and i64 %x, 65535
  ret i64 %r
}

; CHECK-LABEL: mask32:
; CHECK:      zapnot $16, 15, $0
; CHECK-NEXT: ret
define i64 @mask32(i64 %x) {
  %r = and i64 %x, 4294967295
  ret i64 %r
}

; A mask with a hole in it is still byte-granular.
; CHECK-LABEL: high_byte:
; CHECK:      zapnot $16, 2, $0
; CHECK-NEXT: ret
define i64 @high_byte(i64 %x) {
  %r = and i64 %x, 65280
  ret i64 %r
}

; 0xff is byte-granular and also fits and's 8-bit literal.  and is a logic
; operation and zapnot a shift-class one, so this stays an and -- and matches
; what the SelectionDAG path emits for the same input.
; CHECK-LABEL: low_byte:
; CHECK:      and $16, 255, $0
; CHECK-NEXT: ret
define i64 @low_byte(i64 %x) {
  %r = and i64 %x, 255
  ret i64 %r
}

; Not byte-granular: the mask has to be built and applied with a register and.
; CHECK-LABEL: not_bytemask:
; CHECK:      lda $0, 4095($31)
; CHECK-NEXT: and $16, $0, $0
; CHECK-NEXT: ret
define i64 @not_bytemask(i64 %x) {
  %r = and i64 %x, 4095
  ret i64 %r
}

; Zero-extending a byte is the same choice made from the other opcode.
; CHECK-LABEL: zext8:
; CHECK:      and $16, 255, $0
; CHECK-NEXT: ret
define i64 @zext8(i8 %x) {
  %r = zext i8 %x to i64
  ret i64 %r
}
