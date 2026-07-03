; RUN: llc -mtriple=alpha-unknown-linux-gnu < %s | FileCheck %s

; Alpha has no byte-swap instruction, so bswap is expanded to a shift/mask
; sequence.  Check that it selects (rather than failing) and needs no libcall.

declare i64 @llvm.bswap.i64(i64)

; The expansion takes each byte to its mirrored position with a shift and a
; zapnot mask and ors the pieces together, so the checks below pin where each
; byte lands.  An expansion that dropped or duplicated one would still make no
; call, which is why the absence of a call is not what is checked.
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

; The narrower widths are not the 64-bit expansion truncated: each moves only
; the bytes it has, so a 32-bit swap is four bytes and a 16-bit one is two.
; CHECK-LABEL: bswap32:
; CHECK-DAG:   srl {{\$[0-9]+}}, 24,
; CHECK-DAG:   sll $16, 24,
; CHECK-NOT:   jsr
; CHECK:       ret
define i32 @bswap32(i32 %x) {
  %r = call i32 @llvm.bswap.i32(i32 %x)
  ret i32 %r
}

; CHECK-LABEL: bswap16:
; CHECK-DAG:   sll $16, 8,
; CHECK-DAG:   srl {{\$[0-9]+}}, 8,
; CHECK-NOT:   jsr
; CHECK:       ret
define i16 @bswap16(i16 %x) {
  %r = call i16 @llvm.bswap.i16(i16 %x)
  ret i16 %r
}
