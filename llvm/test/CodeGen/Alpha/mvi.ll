; The MVI min/max/pack intrinsics.  All thirteen, because until this test
; existed the CodeGen suite covered exactly two of them -- `minub8`, and only
; incidentally, plus `unpkbw` in intrinsics.ll -- and the other eleven had no
; test at any optimisation level or subtarget.
;
; RUN: llc -mtriple=alpha-unknown-linux-gnu -mattr=+mvi < %s | FileCheck %s
;
; Again through the cpu rather than the raw feature, because `-mcpu=ev67` is
; how a user reaches these and the implication is the part that can break
; without any pattern changing.
; RUN: llc -mtriple=alpha-unknown-linux-gnu -mcpu=ev67 < %s | FileCheck %s
;
; And once with neither, which is what says the +mvi above is load-bearing: a
; feature gate that had stopped gating would let the first two RUN lines pass
; unchanged.
; RUN: not --crash llc -mtriple=alpha-unknown-linux-gnu < %s 2>&1 \
; RUN:   | FileCheck %s --check-prefix=NOMVI
; NOMVI: Cannot select: intrinsic %llvm.alpha.minsb8

; CHECK-LABEL: minsb8:
; CHECK: minsb8 $16, $17, $0
define i64 @minsb8(i64 %a, i64 %b) {
  %r = call i64 @llvm.alpha.minsb8(i64 %a, i64 %b)
  ret i64 %r
}

; CHECK-LABEL: minsw4:
; CHECK: minsw4 $16, $17, $0
define i64 @minsw4(i64 %a, i64 %b) {
  %r = call i64 @llvm.alpha.minsw4(i64 %a, i64 %b)
  ret i64 %r
}

; CHECK-LABEL: minub8:
; CHECK: minub8 $16, $17, $0
define i64 @minub8(i64 %a, i64 %b) {
  %r = call i64 @llvm.alpha.minub8(i64 %a, i64 %b)
  ret i64 %r
}

; CHECK-LABEL: minuw4:
; CHECK: minuw4 $16, $17, $0
define i64 @minuw4(i64 %a, i64 %b) {
  %r = call i64 @llvm.alpha.minuw4(i64 %a, i64 %b)
  ret i64 %r
}

; CHECK-LABEL: maxsb8:
; CHECK: maxsb8 $16, $17, $0
define i64 @maxsb8(i64 %a, i64 %b) {
  %r = call i64 @llvm.alpha.maxsb8(i64 %a, i64 %b)
  ret i64 %r
}

; CHECK-LABEL: maxsw4:
; CHECK: maxsw4 $16, $17, $0
define i64 @maxsw4(i64 %a, i64 %b) {
  %r = call i64 @llvm.alpha.maxsw4(i64 %a, i64 %b)
  ret i64 %r
}

; CHECK-LABEL: maxub8:
; CHECK: maxub8 $16, $17, $0
define i64 @maxub8(i64 %a, i64 %b) {
  %r = call i64 @llvm.alpha.maxub8(i64 %a, i64 %b)
  ret i64 %r
}

; CHECK-LABEL: maxuw4:
; CHECK: maxuw4 $16, $17, $0
define i64 @maxuw4(i64 %a, i64 %b) {
  %r = call i64 @llvm.alpha.maxuw4(i64 %a, i64 %b)
  ret i64 %r
}

; perr is the odd one in the group: it is a sum of absolute differences, not a
; lane-wise min or max, and it shares only the encoding neighbourhood.
; CHECK-LABEL: perr:
; CHECK: perr $16, $17, $0
define i64 @perr(i64 %a, i64 %b) {
  %r = call i64 @llvm.alpha.perr(i64 %a, i64 %b)
  ret i64 %r
}

; The four pack/unpack forms are unary and use the other MVI encoding.
; CHECK-LABEL: pklb:
; CHECK: pklb $16, $0
define i64 @pklb(i64 %a) {
  %r = call i64 @llvm.alpha.pklb(i64 %a)
  ret i64 %r
}

; CHECK-LABEL: pkwb:
; CHECK: pkwb $16, $0
define i64 @pkwb(i64 %a) {
  %r = call i64 @llvm.alpha.pkwb(i64 %a)
  ret i64 %r
}

; CHECK-LABEL: unpkbl:
; CHECK: unpkbl $16, $0
define i64 @unpkbl(i64 %a) {
  %r = call i64 @llvm.alpha.unpkbl(i64 %a)
  ret i64 %r
}

; CHECK-LABEL: unpkbw:
; CHECK: unpkbw $16, $0
define i64 @unpkbw(i64 %a) {
  %r = call i64 @llvm.alpha.unpkbw(i64 %a)
  ret i64 %r
}
