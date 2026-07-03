; RUN: llc -mtriple=alpha-unknown-linux-gnu -mcpu=ev67 < %s \
; RUN:   | FileCheck %s --check-prefix=CIX
; RUN: llc -mtriple=alpha-unknown-linux-gnu -mcpu=ev4 < %s \
; RUN:   | FileCheck %s --check-prefix=NOCIX

declare i64 @llvm.ctpop.i64(i64)
declare i64 @llvm.ctlz.i64(i64, i1)
declare i64 @llvm.cttz.i64(i64, i1)

; With the CIX extension the counts are single instructions.
; CIX-LABEL: pc:
; CIX:       ctpop $16, $0
; Without it, they expand and there is no ctpop.
; NOCIX-LABEL: pc:
; NOCIX-NOT:  ctpop
define i64 @pc(i64 %x) {
  %r = call i64 @llvm.ctpop.i64(i64 %x)
  ret i64 %r
}

; CIX-LABEL: lz:
; CIX:       ctlz $16, $0
define i64 @lz(i64 %x) {
  %r = call i64 @llvm.ctlz.i64(i64 %x, i1 false)
  ret i64 %r
}

; CIX-LABEL: tz:
; CIX:       cttz $16, $0
define i64 @tz(i64 %x) {
  %r = call i64 @llvm.cttz.i64(i64 %x, i1 false)
  ret i64 %r
}

; __builtin_popcount on a 32-bit value: zero-extend the low word, then ctpop.
; CIX-LABEL: pc32:
; CIX:       zapnot $16, 15, $0
; CIX-NEXT:  ctpop $0, $0
define i32 @pc32(i32 %x) {
  %r = call i32 @llvm.ctpop.i32(i32 %x)
  ret i32 %r
}

; __builtin_clz on a 32-bit value (undefined at zero): shift the value into the
; high word so its leading zeros are the 32-bit count, then ctlz.
; CIX-LABEL: lz32:
; CIX:       sll $16, 32, $0
; CIX-NEXT:  ctlz $0, $0
define i32 @lz32(i32 %x) {
  %r = call i32 @llvm.ctlz.i32(i32 %x, i1 true)
  ret i32 %r
}

; __builtin_ctz on a 32-bit value (undefined at zero) is a plain cttz.
; CIX-LABEL: tz32:
; CIX:       cttz $16, $0
define i32 @tz32(i32 %x) {
  %r = call i32 @llvm.cttz.i32(i32 %x, i1 true)
  ret i32 %r
}

; __builtin_parity is the population count masked to its low bit.
; CIX-LABEL: parity:
; CIX:       ctpop $16, $0
; CIX-NEXT:  and $0, 1, $0
define i64 @parity(i64 %x) {
  %p = call i64 @llvm.ctpop.i64(i64 %x)
  %r = and i64 %p, 1
  ret i64 %r
}
