; RUN: llc -mtriple=alpha-unknown-linux-gnu < %s | FileCheck %s

; Alpha has no byte-swap instruction, so bswap is expanded to a shift/mask
; sequence.  Check that it selects (rather than failing) and needs no libcall.

declare i64 @llvm.bswap.i64(i64)

; The expansion takes each byte to its mirrored position with a shift and a
; zapnot mask and ors the pieces together.  Checking only for the absence of a
; call said nothing about whether the bytes end up where they belong: an
; expansion that dropped or duplicated a byte would pass that just as well.
; CHECK-LABEL: bswap64:
; CHECK-DAG:   srl $16, 56,
; CHECK-DAG:   srl $16, 40,
; CHECK-DAG:   srl $16, 24,
; CHECK-DAG:   srl $16, 8,
; CHECK-DAG:   sll $16, 56,
; CHECK-DAG:   sll {{\$[0-9]+}}, 40,
; CHECK-DAG:   sll {{\$[0-9]+}}, 24,
; CHECK-DAG:   sll {{\$[0-9]+}}, 8,
; CHECK-NOT:   jsr
; CHECK:       ret
define i64 @bswap64(i64 %x) {
  %r = call i64 @llvm.bswap.i64(i64 %x)
  ret i64 %r
}
