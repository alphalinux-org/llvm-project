; RUN: llc -mtriple=alpha-unknown-linux-gnu -relocation-model=pic -O2 < %s \
; RUN:   | FileCheck %s
; RUN: llc -mtriple=alpha-unknown-linux-gnu -relocation-model=pic -O2 \
; RUN:   -filetype=obj < %s | llvm-readobj -r - | FileCheck %s --check-prefix=RELOC

; General-dynamic TLS: pass the tlsgd descriptor (lda !tlsgd) to __tls_get_addr,
; whose return value in $0 is the variable's address.  The jsr carries a
; lituse_tlsgd relocation (R_ALPHA_LITUSE with addend 4) so the linker can relax
; the sequence to initial- or local-exec.

@gd = external thread_local global i32

; CHECK-LABEL: read_gd:
; CHECK:      ldgp $29, 0($27)
; CHECK:      lda $16, gd($29)		!tlsgd
; CHECK:      ldq $27, __tls_get_addr($29)		!literal
; CHECK:      jsr $26, ($27)
; CHECK:      ldgp $29, 0($26)
; CHECK:      ldl {{\$[0-9]+}}, 0($0)

; The thread pointer is not read on this path: __tls_get_addr hands back the
; address, in the very register call_pal rduniq would have written.
; CHECK-NOT:  call_pal

; RELOC:      R_ALPHA_TLSGD gd
; RELOC:      R_ALPHA_LITERAL __tls_get_addr
; RELOC:      R_ALPHA_LITUSE - 0x4
define i32 @read_gd() {
  %p = call ptr @llvm.threadlocal.address.p0(ptr @gd)
  %v = load i32, ptr %p
  ret i32 %v
}
