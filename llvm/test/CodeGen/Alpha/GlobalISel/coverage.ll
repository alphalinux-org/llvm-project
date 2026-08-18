; RUN: llc -mtriple=alpha-unknown-linux-gnu -global-isel -global-isel-abort=1 < %s \
; RUN:   | FileCheck %s --check-prefixes=CHECK,BASE --implicit-check-not=ctpop \
; RUN:     --implicit-check-not=ctlz --implicit-check-not=cttz \
; RUN:     --implicit-check-not=sqrtt --implicit-check-not=ftoit
; RUN: llc -mtriple=alpha-unknown-linux-gnu -mcpu=ev67 -global-isel \
; RUN:   -global-isel-abort=1 < %s | FileCheck %s --check-prefixes=CHECK,EV67

; Constructs a C program produces that the legalizer had no rule for, so they
; fell out of GlobalISel at run time with "unable to legalize".  -global-isel-abort=1
; is the point of the test: without it an unlegalizable function quietly falls
; back to SelectionDAG and the gap stays invisible.
;
; The second run line is ev67, where sqrt is an instruction rather than an
; expansion, so both sides of that rule are covered.
;
; A switch dense enough to become a jump table is deliberately absent: the
; dispatch is the gp-relative sequence LowerBR_JT builds and the selector has
; no counterpart, so G_BRJT is marked unsupported and such a function goes to
; the SelectionDAG path whole.  The address of a block or of a constant-pool
; entry is left the same way, for the same reason, as are the atomics, whose
; retry loop the SelectionDAG path builds with a custom inserter that
; GlobalISel does not run.  See fallback.ll.

; CHECK-LABEL: fence:
; CHECK:      mb
; CHECK-NEXT: ret
define void @fence() {
  fence seq_cst
  ret void
}

; An undef the freeze turns into a concrete value leaves nothing behind: $0 is
; whatever it already held.
; CHECK-LABEL: undef_freeze:
; CHECK:      %bb.0:
; CHECK-NEXT: ret
define i64 @undef_freeze() {
  %u = freeze i64 undef
  ret i64 %u
}

; CHECK-LABEL: bitcast_i64_f64:
; BASE:      lda $30, -16($30)
; BASE:      stt $f16, 8($30)
; BASE-NEXT: ldq $0, 8($30)
; BASE-NEXT: lda $30, 16($30)
; BASE:      ret
; EV67:      ftoit $f16, $0
; EV67-NEXT: ret
define i64 @bitcast_i64_f64(double %x) {
  %r = bitcast double %x to i64
  ret i64 %r
}

; fabs is a copy with the sign bit taken from $f31, and the fneg that follows
; flips it back with cpysn.
; CHECK-LABEL: fneg_fabs:
; CHECK:      cpys $f31, $f16, $f0
; CHECK-NEXT: cpysn $f0, $f0, $f0
; CHECK-NEXT: ret
define double @fneg_fabs(double %x) {
  %a = call double @llvm.fabs.f64(double %x)
  %n = fneg double %a
  ret double %n
}

; CHECK-LABEL: fsqrt:
; BASE:      ldq $27, sqrt($29)
; BASE:      jsr $26, ($27)
; EV67:      sqrtt $f16, $f0
; EV67-NEXT: ret
define double @fsqrt(double %x) {
  %r = call double @llvm.sqrt.f64(double %x)
  ret double %r
}

; CHECK-LABEL: overflow:
; CHECK:      addq $16, $17, $0
; CHECK-NEXT: ret
define i64 @overflow(i64 %a, i64 %b) {
  %p = call {i64, i1} @llvm.uadd.with.overflow.i64(i64 %a, i64 %b)
  %v = extractvalue {i64, i1} %p, 0
  ret i64 %v
}

; The high half of a 64x64 unsigned multiply: umulh plus the two cross products,
; which are multiplies by the zero high halves the zexts produced.
; CHECK-LABEL: wide_i128:
; CHECK:      lda [[Z:\$[0-9]+]], 0($31)
; CHECK-NEXT: umulh $16, $17, [[H:\$[0-9]+]]
; CHECK-NEXT: mulq $16, 0, [[A:\$[0-9]+]]
; CHECK-NEXT: mulq [[Z]], $17, [[B:\$[0-9]+]]
; CHECK-NEXT: addq [[H]], [[A]], [[H]]
; CHECK-NEXT: addq [[H]], [[B]], $0
; CHECK-NEXT: ret
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
declare {i64, i1} @llvm.uadd.with.overflow.i64(i64, i64)
