; RUN: llc -mtriple=alpha-unknown-linux-gnu -relocation-model=pic -O2 < %s \
; RUN:   | FileCheck %s
; RUN: llc -mtriple=alpha-unknown-linux-gnu -relocation-model=pic -O2 \
; RUN:   -filetype=obj < %s | llvm-readobj -r - | FileCheck %s --check-prefix=RELOC

; And through the printed form: the descriptor, the literal and the call are
; tied together by one sequence number, which the text has to carry.
; RUN: llc -mtriple=alpha-unknown-linux-gnu -relocation-model=pic -O2 < %s \
; RUN:   | llvm-mc -triple=alpha-unknown-linux-gnu -filetype=obj -o - \
; RUN:   | llvm-readobj -r - | FileCheck %s --check-prefix=RELOC

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
; CHECK:      ldl {{\$[0-9]+}}, ld({{\$[0-9]+}})		!dtprello

; The thread pointer is not read on this path: __tls_get_addr hands back the
; address, in the very register call_pal rduniq would have written.
; CHECK-NOT:  call_pal

; RELOC:      R_ALPHA_TLSLDM ld
; RELOC:      R_ALPHA_LITERAL __tls_get_addr
; RELOC:      R_ALPHA_LITUSE - 0x5
define i32 @read_ld() {
  %p = call ptr @llvm.threadlocal.address.p0(ptr @ld)
  %v = load i32, ptr %p
  ret i32 %v
}
