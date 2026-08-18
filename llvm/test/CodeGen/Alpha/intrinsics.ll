; RUN: llc -mtriple=alpha-unknown-linux-gnu -mattr=+cix,+mvi < %s | FileCheck %s

; CHECK-LABEL: test_implver:
; CHECK: implver $0
define i64 @test_implver() {
  %r = call i64 @llvm.alpha.implver()
  ret i64 %r
}

; CHECK-LABEL: test_rpcc:
; CHECK: rpcc $0
define i64 @test_rpcc() {
  %r = call i64 @llvm.alpha.rpcc()
  ret i64 %r
}

; CHECK-LABEL: test_amask:
; CHECK: amask $16, $0
define i64 @test_amask(i64 %x) {
  %r = call i64 @llvm.alpha.amask(i64 %x)
  ret i64 %r
}

; CHECK-LABEL: test_cmpbge:
; CHECK: cmpbge $16, $17, $0
define i64 @test_cmpbge(i64 %x, i64 %y) {
  %r = call i64 @llvm.alpha.cmpbge(i64 %x, i64 %y)
  ret i64 %r
}

; CHECK-LABEL: test_umulh:
; CHECK: umulh $16, $17, $0
define i64 @test_umulh(i64 %x, i64 %y) {
  %r = call i64 @llvm.alpha.umulh(i64 %x, i64 %y)
  ret i64 %r
}

; CHECK-LABEL: test_zap:
; CHECK: zap $16, $17, $0
define i64 @test_zap(i64 %x, i64 %y) {
  %r = call i64 @llvm.alpha.zap(i64 %x, i64 %y)
  ret i64 %r
}

; CHECK-LABEL: test_zapnot:
; CHECK: zapnot $16, $17, $0
define i64 @test_zapnot(i64 %x, i64 %y) {
  %r = call i64 @llvm.alpha.zapnot(i64 %x, i64 %y)
  ret i64 %r
}

; CHECK-LABEL: test_extqh:
; CHECK: extqh $16, $17, $0
define i64 @test_extqh(i64 %x, i64 %y) {
  %r = call i64 @llvm.alpha.extqh(i64 %x, i64 %y)
  ret i64 %r
}

; CHECK-LABEL: test_insql:
; CHECK: insql $16, $17, $0
define i64 @test_insql(i64 %x, i64 %y) {
  %r = call i64 @llvm.alpha.insql(i64 %x, i64 %y)
  ret i64 %r
}

; CHECK-LABEL: test_mskwh:
; CHECK: mskwh $16, $17, $0
define i64 @test_mskwh(i64 %x, i64 %y) {
  %r = call i64 @llvm.alpha.mskwh(i64 %x, i64 %y)
  ret i64 %r
}

; CHECK-LABEL: test_ctpop:
; CHECK: ctpop $16, $0
define i64 @test_ctpop(i64 %x) {
  %r = call i64 @llvm.alpha.ctpop(i64 %x)
  ret i64 %r
}

; CHECK-LABEL: test_minub8:
; CHECK: minub8 $16, $17, $0
define i64 @test_minub8(i64 %x, i64 %y) {
  %r = call i64 @llvm.alpha.minub8(i64 %x, i64 %y)
  ret i64 %r
}

; CHECK-LABEL: test_unpkbw:
; CHECK: unpkbw $16, $0
define i64 @test_unpkbw(i64 %x) {
  %r = call i64 @llvm.alpha.unpkbw(i64 %x)
  ret i64 %r
}
