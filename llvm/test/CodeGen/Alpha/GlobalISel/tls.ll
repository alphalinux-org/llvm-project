; RUN: llc -mtriple=alpha-unknown-linux-gnu -relocation-model=pic -global-isel \
; RUN:   -global-isel-abort=0 < %s | FileCheck %s

; A thread-local variable's address is not the address of its template in
; .tdata but an offset into the running thread's block, which takes the
; tlsgd/tlsldm/gottprel/tprel sequences.  The selector has none of them, so it
; refuses the global and the function goes to the SelectionDAG path, which
; does.  Addressing one as an ordinary global read the template instead --
; silently for a local variable, and as a TLS-versus-non-TLS symbol type
; mismatch at link time for an imported one.

@g = external thread_local global i64
@l = internal thread_local global i64 5
@ie = external thread_local(initialexec) global i64
@le = internal thread_local(localexec) global i64 7

define i64 @general_dynamic() {
; CHECK-LABEL: general_dynamic:
; CHECK: lda $16, g($29){{.*}}!tlsgd
; CHECK: jsr $26, ($27){{.*}}!lituse_tlsgd
  %p = call ptr @llvm.threadlocal.address.p0(ptr @g)
  %v = load i64, ptr %p
  ret i64 %v
}

define i64 @local_dynamic() {
; CHECK-LABEL: local_dynamic:
; CHECK: lda $16, l($29){{.*}}!tlsldm
; CHECK: ldah $0, l($0){{.*}}!dtprelhi
  %p = call ptr @llvm.threadlocal.address.p0(ptr @l)
  %v = load i64, ptr %p
  ret i64 %v
}

define i64 @initial_exec() {
; CHECK-LABEL: initial_exec:
; CHECK: ldq {{\$[0-9]+}}, ie($29){{.*}}!gottprel
  %p = call ptr @llvm.threadlocal.address.p0(ptr @ie)
  %v = load i64, ptr %p
  ret i64 %v
}

define i64 @local_exec() {
; CHECK-LABEL: local_exec:
; CHECK: ldah $0, le($0){{.*}}!tprelhi
; CHECK: ldq $0, le($0){{.*}}!tprello
  %p = call ptr @llvm.threadlocal.address.p0(ptr @le)
  %v = load i64, ptr %p
  ret i64 %v
}

declare ptr @llvm.threadlocal.address.p0(ptr)
