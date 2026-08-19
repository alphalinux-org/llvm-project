; RUN: llc -mtriple=alpha-unknown-linux-gnu -O2 < %s | FileCheck %s

declare ptr @llvm.stacksave()
declare void @llvm.stackrestore(ptr)
declare void @use(ptr)

; CHECK-LABEL: f:
; CHECK:      bis $31, $30, {{\$[0-9]+}}
; CHECK:      bis $31, {{\$[0-9]+}}, $30
define void @f(i64 %n) {
  %s = call ptr @llvm.stacksave()
  %p = alloca i64, i64 %n
  call void @use(ptr %p)
  call void @llvm.stackrestore(ptr %s)
  ret void
}
