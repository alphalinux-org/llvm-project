; RUN: llc -mtriple=alpha-unknown-linux-gnu -relocation-model=static -O2 < %s \
; RUN:   | FileCheck %s

; Local-exec TLS: read the thread pointer with the rduniq PALcall, then add the
; link-time constant offset formed with ldah !tprelhi / lda !tprello.

@le = internal thread_local global i32 42
@leq = internal thread_local global i64 7

; CHECK-LABEL: read:
; CHECK:      call_pal 0x9e
; CHECK:      ldah {{\$[0-9]+}}, le({{\$[0-9]+}})		!tprelhi
; CHECK:      lda {{\$[0-9]+}}, le({{\$[0-9]+}})		!tprello
; CHECK:      ldl {{\$[0-9]+}}, 0({{\$[0-9]+}})
define i32 @read() {
  %p = call ptr @llvm.threadlocal.address.p0(ptr @le)
  %v = load i32, ptr %p
  ret i32 %v
}

; A store folds the !tprello low part into the store displacement.
; CHECK-LABEL: write:
; CHECK:      call_pal 0x9e
; CHECK:      ldah {{\$[0-9]+}}, le({{\$[0-9]+}})		!tprelhi
; CHECK:      stl $16, le({{\$[0-9]+}})		!tprello
define void @write(i32 %x) {
  %p = call ptr @llvm.threadlocal.address.p0(ptr @le)
  store i32 %x, ptr %p
  ret void
}

; A 64-bit load folds the !tprello into the ldq.
; CHECK-LABEL: readq:
; CHECK:      call_pal 0x9e
; CHECK:      ldah {{\$[0-9]+}}, leq({{\$[0-9]+}})		!tprelhi
; CHECK:      ldq {{\$[0-9]+}}, leq({{\$[0-9]+}})		!tprello
define i64 @readq() {
  %p = call ptr @llvm.threadlocal.address.p0(ptr @leq)
  %v = load i64, ptr %p
  ret i64 %v
}
