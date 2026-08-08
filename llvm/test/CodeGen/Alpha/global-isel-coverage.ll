; RUN: llc -mtriple=alpha-unknown-linux-gnu -global-isel -global-isel-abort=1 < %s | FileCheck %s
; RUN: llc -mtriple=alpha-unknown-linux-gnu -mcpu=ev67 -global-isel \
; RUN:   -global-isel-abort=1 < %s | FileCheck %s

; Constructs a C program produces that the legalizer had no rule for, so they
; fell out of GlobalISel at run time with "unable to legalize".  -global-isel-abort=1
; is the point of the test: without it an unlegalizable function quietly falls
; back to SelectionDAG and the gap stays invisible.
;
; The second run line is ev67, where ctpop/ctlz/cttz and sqrt are instructions
; rather than expansions, so both sides of those rules are covered.
;
; A switch dense enough to become a jump table is deliberately absent: the
; dispatch is the gp-relative sequence LowerBR_JT builds and the selector has
; no counterpart, so G_BRJT is marked unsupported and such a function goes to
; the SelectionDAG path whole.  The address of a block or of a constant-pool
; entry is left the same way, for the same reason, as are the atomics, whose
; retry loop the SelectionDAG path builds with a custom inserter that
; GlobalISel does not run.  See global-isel-fallback.ll.

; CHECK-LABEL: fence:
define void @fence() {
  fence seq_cst
  ret void
}

; CHECK-LABEL: undef_freeze:
define i64 @undef_freeze() {
  %u = freeze i64 undef
  ret i64 %u
}

; CHECK-LABEL: bitcast_i64_f64:
define i64 @bitcast_i64_f64(double %x) {
  %r = bitcast double %x to i64
  ret i64 %r
}

; CHECK-LABEL: fneg_fabs:
define double @fneg_fabs(double %x) {
  %a = call double @llvm.fabs.f64(double %x)
  %n = fneg double %a
  ret double %n
}

; CHECK-LABEL: fsqrt:
define double @fsqrt(double %x) {
  %r = call double @llvm.sqrt.f64(double %x)
  ret double %r
}

; CHECK-LABEL: counts:
define i64 @counts(i64 %x) {
  %a = call i64 @llvm.ctpop.i64(i64 %x)
  %b = call i64 @llvm.ctlz.i64(i64 %x, i1 false)
  %c = call i64 @llvm.cttz.i64(i64 %x, i1 false)
  %d = add i64 %a, %b
  %e = add i64 %d, %c
  ret i64 %e
}

; CHECK-LABEL: bswap_abs:
define i64 @bswap_abs(i64 %x) {
  %a = call i64 @llvm.bswap.i64(i64 %x)
  %b = call i64 @llvm.abs.i64(i64 %a, i1 false)
  ret i64 %b
}

; CHECK-LABEL: minmax:
define i64 @minmax(i64 %a, i64 %b) {
  %x = call i64 @llvm.smax.i64(i64 %a, i64 %b)
  %y = call i64 @llvm.umin.i64(i64 %x, i64 %b)
  ret i64 %y
}

; CHECK-LABEL: overflow:
define i64 @overflow(i64 %a, i64 %b) {
  %p = call {i64, i1} @llvm.uadd.with.overflow.i64(i64 %a, i64 %b)
  %v = extractvalue {i64, i1} %p, 0
  ret i64 %v
}

; CHECK-LABEL: wide_i128:
define i64 @wide_i128(i64 %a, i64 %b) {
  %x = zext i64 %a to i128
  %y = zext i64 %b to i128
  %m = mul i128 %x, %y
  %s = lshr i128 %m, 64
  %r = trunc i128 %s to i64
  ret i64 %r
}

declare double @llvm.fabs.f64(double)
declare double @llvm.sqrt.f64(double)
declare i64 @llvm.ctpop.i64(i64)
declare i64 @llvm.ctlz.i64(i64, i1)
declare i64 @llvm.cttz.i64(i64, i1)
declare i64 @llvm.bswap.i64(i64)
declare i64 @llvm.abs.i64(i64, i1)
declare i64 @llvm.smax.i64(i64, i64)
declare i64 @llvm.umin.i64(i64, i64)
declare {i64, i1} @llvm.uadd.with.overflow.i64(i64, i64)
