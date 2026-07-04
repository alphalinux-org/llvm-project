; RUN: llc -mtriple=alpha-unknown-linux-gnu -relocation-model=pic -O2 < %s \
; RUN:   | FileCheck %s
; RUN: llc -mtriple=alpha-unknown-linux-gnu -relocation-model=pic -O2 \
; RUN:   -filetype=obj < %s | llvm-readobj -r - | FileCheck %s --check-prefix=RELOC

; Local-dynamic TLS: pass the tlsldm module descriptor to __tls_get_addr for the
; module's TLS base, then add the variable's module-relative offset formed with
; ldah !dtprelhi / lda !dtprello.  The jsr carries a lituse_tlsldm relocation
; (R_ALPHA_LITUSE with addend 5) so the linker can relax the sequence.

@ld = internal thread_local global i32 5

; CHECK-LABEL: read_ld:
; CHECK:      lda $16, ld($29)		!tlsldm
; CHECK:      ldq $27, __tls_get_addr($29)		!literal
; CHECK:      jsr $26, ($27)
; CHECK:      ldah {{\$[0-9]+}}, ld($0)		!dtprelhi
; CHECK:      lda {{\$[0-9]+}}, ld({{\$[0-9]+}})		!dtprello

; RELOC:      R_ALPHA_TLSLDM ld
; RELOC:      R_ALPHA_LITERAL __tls_get_addr
; RELOC:      R_ALPHA_LITUSE .text 0x5
define i32 @read_ld() {
  %p = call ptr @llvm.threadlocal.address.p0(ptr @ld)
  %v = load i32, ptr %p
  ret i32 %v
}
