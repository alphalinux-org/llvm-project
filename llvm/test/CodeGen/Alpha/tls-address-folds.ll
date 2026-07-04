; RUN: llc -mtriple=alpha-unknown-linux-gnu -O2 < %s | FileCheck %s

; The low half of a thread-pointer-relative address is a displacement, so a
; load or a store using it folds the lda away and carries the !tprello itself.

@i32v = internal thread_local(localexec) global i32 0
@i64v = internal thread_local(localexec) global i64 0
@f32v = internal thread_local(localexec) global float 0.0
@f64v = internal thread_local(localexec) global double 0.0

declare ptr @llvm.threadlocal.address.p0(ptr)

; CHECK-LABEL: ld_i32:
; CHECK:      ldah $0, i32v($0){{.*}}!tprelhi
; CHECK-NEXT: ldl $0, i32v($0){{.*}}!tprello
; CHECK-NOT:  lda
define i64 @ld_i32() {
  %p = call ptr @llvm.threadlocal.address.p0(ptr @i32v)
  %v = load i32, ptr %p
  %r = sext i32 %v to i64
  ret i64 %r
}

; CHECK-LABEL: ld_i64:
; CHECK:      ldq $0, i64v($0){{.*}}!tprello
define i64 @ld_i64() {
  %p = call ptr @llvm.threadlocal.address.p0(ptr @i64v)
  %v = load i64, ptr %p
  ret i64 %v
}

; A floating load folds the same way, into lds and ldt.
; CHECK-LABEL: ld_f32:
; CHECK:      lds $f0, f32v($0){{.*}}!tprello
define float @ld_f32() {
  %p = call ptr @llvm.threadlocal.address.p0(ptr @f32v)
  %v = load float, ptr %p
  ret float %v
}

; CHECK-LABEL: ld_f64:
; CHECK:      ldt $f0, f64v($0){{.*}}!tprello
define double @ld_f64() {
  %p = call ptr @llvm.threadlocal.address.p0(ptr @f64v)
  %v = load double, ptr %p
  ret double %v
}

; CHECK-LABEL: st_i32:
; CHECK:      stl $16, i32v($0){{.*}}!tprello
define void @st_i32(i32 %v) {
  %p = call ptr @llvm.threadlocal.address.p0(ptr @i32v)
  store i32 %v, ptr %p
  ret void
}

; CHECK-LABEL: st_i64:
; CHECK:      stq $16, i64v($0){{.*}}!tprello
define void @st_i64(i64 %v) {
  %p = call ptr @llvm.threadlocal.address.p0(ptr @i64v)
  store i64 %v, ptr %p
  ret void
}
