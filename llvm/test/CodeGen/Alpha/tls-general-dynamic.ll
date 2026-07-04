; RUN: llc -mtriple=alpha-unknown-linux-gnu -relocation-model=pic -O2 < %s \
; RUN:   | FileCheck %s

; General-dynamic TLS: pass the tlsgd descriptor (lda !tlsgd) to __tls_get_addr,
; whose return value in $0 is the variable's address.

@gd = external thread_local global i32

; CHECK-LABEL: read_gd:
; CHECK:      ldgp $29, 0($27)
; CHECK:      lda $16, gd($29)		!tlsgd
; CHECK:      ldq $27, __tls_get_addr($29)		!literal
; CHECK:      jsr $26, ($27)
; CHECK:      ldgp $29, 0($26)
; CHECK:      ldl {{\$[0-9]+}}, 0($0)
define i32 @read_gd() {
  %p = call ptr @llvm.threadlocal.address.p0(ptr @gd)
  %v = load i32, ptr %p
  ret i32 %v
}
