; RUN: llc -mtriple=alpha-unknown-linux-gnu -relocation-model=pic -O2 < %s \
; RUN:   | FileCheck %s

; Local-dynamic TLS: pass the tlsldm module descriptor to __tls_get_addr for the
; module's TLS base, then add the variable's module-relative offset formed with
; ldah !dtprelhi / lda !dtprello.

@ld = internal thread_local global i32 5

; CHECK-LABEL: read_ld:
; CHECK:      lda $16, ld($29)		!tlsldm
; CHECK:      ldq $27, __tls_get_addr($29)		!literal
; CHECK:      jsr $26, ($27)
; CHECK:      ldah {{\$[0-9]+}}, ld($0)		!dtprelhi
; CHECK:      lda {{\$[0-9]+}}, ld({{\$[0-9]+}})		!dtprello
define i32 @read_ld() {
  %p = call ptr @llvm.threadlocal.address.p0(ptr @ld)
  %v = load i32, ptr %p
  ret i32 %v
}
