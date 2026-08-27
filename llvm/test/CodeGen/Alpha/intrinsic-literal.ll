; The literal operand form of every __builtin_alpha_* intrinsic that takes one.
; An operate instruction takes an 8-bit literal wherever it takes $Rb, and these
; builtins are written with a constant far more often than with a variable -- a
; byte position, a zap mask, a shift amount.  Before these patterns existed the
; constant was materialized into a register first, so every one of these cost an
; `lda` that gcc does not emit; the CHECK-NOT is the whole point of the test.

; RUN: llc -mtriple=alpha-unknown-linux-gnu -mattr=+mvi < %s | FileCheck %s

; CHECK-LABEL: cmpbge:
; CHECK-NOT: lda
; CHECK: cmpbge $16, 3, $0
define i64 @cmpbge(i64 %a) {
  %r = call i64 @llvm.alpha.cmpbge(i64 %a, i64 3)
  ret i64 %r
}

; CHECK-LABEL: zap:
; CHECK-NOT: lda
; CHECK: zap $16, 3, $0
define i64 @zap(i64 %a) {
  %r = call i64 @llvm.alpha.zap(i64 %a, i64 3)
  ret i64 %r
}

; CHECK-LABEL: zapnot:
; CHECK-NOT: lda
; CHECK: zapnot $16, 3, $0
define i64 @zapnot(i64 %a) {
  %r = call i64 @llvm.alpha.zapnot(i64 %a, i64 3)
  ret i64 %r
}

; CHECK-LABEL: extbl:
; CHECK-NOT: lda
; CHECK: extbl $16, 3, $0
define i64 @extbl(i64 %a) {
  %r = call i64 @llvm.alpha.extbl(i64 %a, i64 3)
  ret i64 %r
}

; CHECK-LABEL: extwl:
; CHECK-NOT: lda
; CHECK: extwl $16, 3, $0
define i64 @extwl(i64 %a) {
  %r = call i64 @llvm.alpha.extwl(i64 %a, i64 3)
  ret i64 %r
}

; CHECK-LABEL: extll:
; CHECK-NOT: lda
; CHECK: extll $16, 3, $0
define i64 @extll(i64 %a) {
  %r = call i64 @llvm.alpha.extll(i64 %a, i64 3)
  ret i64 %r
}

; CHECK-LABEL: extql:
; CHECK-NOT: lda
; CHECK: extql $16, 3, $0
define i64 @extql(i64 %a) {
  %r = call i64 @llvm.alpha.extql(i64 %a, i64 3)
  ret i64 %r
}

; CHECK-LABEL: extwh:
; CHECK-NOT: lda
; CHECK: extwh $16, 3, $0
define i64 @extwh(i64 %a) {
  %r = call i64 @llvm.alpha.extwh(i64 %a, i64 3)
  ret i64 %r
}

; CHECK-LABEL: extlh:
; CHECK-NOT: lda
; CHECK: extlh $16, 3, $0
define i64 @extlh(i64 %a) {
  %r = call i64 @llvm.alpha.extlh(i64 %a, i64 3)
  ret i64 %r
}

; CHECK-LABEL: extqh:
; CHECK-NOT: lda
; CHECK: extqh $16, 3, $0
define i64 @extqh(i64 %a) {
  %r = call i64 @llvm.alpha.extqh(i64 %a, i64 3)
  ret i64 %r
}

; CHECK-LABEL: insbl:
; CHECK-NOT: lda
; CHECK: insbl $16, 3, $0
define i64 @insbl(i64 %a) {
  %r = call i64 @llvm.alpha.insbl(i64 %a, i64 3)
  ret i64 %r
}

; CHECK-LABEL: inswl:
; CHECK-NOT: lda
; CHECK: inswl $16, 3, $0
define i64 @inswl(i64 %a) {
  %r = call i64 @llvm.alpha.inswl(i64 %a, i64 3)
  ret i64 %r
}

; CHECK-LABEL: insll:
; CHECK-NOT: lda
; CHECK: insll $16, 3, $0
define i64 @insll(i64 %a) {
  %r = call i64 @llvm.alpha.insll(i64 %a, i64 3)
  ret i64 %r
}

; CHECK-LABEL: insql:
; CHECK-NOT: lda
; CHECK: insql $16, 3, $0
define i64 @insql(i64 %a) {
  %r = call i64 @llvm.alpha.insql(i64 %a, i64 3)
  ret i64 %r
}

; CHECK-LABEL: inswh:
; CHECK-NOT: lda
; CHECK: inswh $16, 3, $0
define i64 @inswh(i64 %a) {
  %r = call i64 @llvm.alpha.inswh(i64 %a, i64 3)
  ret i64 %r
}

; CHECK-LABEL: inslh:
; CHECK-NOT: lda
; CHECK: inslh $16, 3, $0
define i64 @inslh(i64 %a) {
  %r = call i64 @llvm.alpha.inslh(i64 %a, i64 3)
  ret i64 %r
}

; CHECK-LABEL: insqh:
; CHECK-NOT: lda
; CHECK: insqh $16, 3, $0
define i64 @insqh(i64 %a) {
  %r = call i64 @llvm.alpha.insqh(i64 %a, i64 3)
  ret i64 %r
}

; CHECK-LABEL: mskbl:
; CHECK-NOT: lda
; CHECK: mskbl $16, 3, $0
define i64 @mskbl(i64 %a) {
  %r = call i64 @llvm.alpha.mskbl(i64 %a, i64 3)
  ret i64 %r
}

; CHECK-LABEL: mskwl:
; CHECK-NOT: lda
; CHECK: mskwl $16, 3, $0
define i64 @mskwl(i64 %a) {
  %r = call i64 @llvm.alpha.mskwl(i64 %a, i64 3)
  ret i64 %r
}

; CHECK-LABEL: mskll:
; CHECK-NOT: lda
; CHECK: mskll $16, 3, $0
define i64 @mskll(i64 %a) {
  %r = call i64 @llvm.alpha.mskll(i64 %a, i64 3)
  ret i64 %r
}

; CHECK-LABEL: mskql:
; CHECK-NOT: lda
; CHECK: mskql $16, 3, $0
define i64 @mskql(i64 %a) {
  %r = call i64 @llvm.alpha.mskql(i64 %a, i64 3)
  ret i64 %r
}

; CHECK-LABEL: mskwh:
; CHECK-NOT: lda
; CHECK: mskwh $16, 3, $0
define i64 @mskwh(i64 %a) {
  %r = call i64 @llvm.alpha.mskwh(i64 %a, i64 3)
  ret i64 %r
}

; CHECK-LABEL: msklh:
; CHECK-NOT: lda
; CHECK: msklh $16, 3, $0
define i64 @msklh(i64 %a) {
  %r = call i64 @llvm.alpha.msklh(i64 %a, i64 3)
  ret i64 %r
}

; CHECK-LABEL: mskqh:
; CHECK-NOT: lda
; CHECK: mskqh $16, 3, $0
define i64 @mskqh(i64 %a) {
  %r = call i64 @llvm.alpha.mskqh(i64 %a, i64 3)
  ret i64 %r
}

; CHECK-LABEL: umulh:
; CHECK-NOT: lda
; CHECK: umulh $16, 3, $0
define i64 @umulh(i64 %a) {
  %r = call i64 @llvm.alpha.umulh(i64 %a, i64 3)
  ret i64 %r
}

; CHECK-LABEL: minsb8:
; CHECK-NOT: lda
; CHECK: minsb8 $16, 3, $0
define i64 @minsb8(i64 %a) {
  %r = call i64 @llvm.alpha.minsb8(i64 %a, i64 3)
  ret i64 %r
}

; CHECK-LABEL: minsw4:
; CHECK-NOT: lda
; CHECK: minsw4 $16, 3, $0
define i64 @minsw4(i64 %a) {
  %r = call i64 @llvm.alpha.minsw4(i64 %a, i64 3)
  ret i64 %r
}

; CHECK-LABEL: minub8:
; CHECK-NOT: lda
; CHECK: minub8 $16, 3, $0
define i64 @minub8(i64 %a) {
  %r = call i64 @llvm.alpha.minub8(i64 %a, i64 3)
  ret i64 %r
}

; CHECK-LABEL: minuw4:
; CHECK-NOT: lda
; CHECK: minuw4 $16, 3, $0
define i64 @minuw4(i64 %a) {
  %r = call i64 @llvm.alpha.minuw4(i64 %a, i64 3)
  ret i64 %r
}

; CHECK-LABEL: maxsb8:
; CHECK-NOT: lda
; CHECK: maxsb8 $16, 3, $0
define i64 @maxsb8(i64 %a) {
  %r = call i64 @llvm.alpha.maxsb8(i64 %a, i64 3)
  ret i64 %r
}

; CHECK-LABEL: maxsw4:
; CHECK-NOT: lda
; CHECK: maxsw4 $16, 3, $0
define i64 @maxsw4(i64 %a) {
  %r = call i64 @llvm.alpha.maxsw4(i64 %a, i64 3)
  ret i64 %r
}

; CHECK-LABEL: maxub8:
; CHECK-NOT: lda
; CHECK: maxub8 $16, 3, $0
define i64 @maxub8(i64 %a) {
  %r = call i64 @llvm.alpha.maxub8(i64 %a, i64 3)
  ret i64 %r
}

; CHECK-LABEL: maxuw4:
; CHECK-NOT: lda
; CHECK: maxuw4 $16, 3, $0
define i64 @maxuw4(i64 %a) {
  %r = call i64 @llvm.alpha.maxuw4(i64 %a, i64 3)
  ret i64 %r
}

; amask has no $Ra operand -- the register form reads $Rb and holds $Ra at $31 --
; so its literal form takes the mask alone and is the one case not covered by the
; shared pattern class.
; CHECK-LABEL: amask:
; CHECK-NOT: lda
; CHECK: amask 3, $0
define i64 @amask() {
  %r = call i64 @llvm.alpha.amask(i64 3)
  ret i64 %r
}
