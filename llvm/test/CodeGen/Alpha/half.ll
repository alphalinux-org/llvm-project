; RUN: llc -mtriple=alpha-unknown-linux-gnu < %s | FileCheck %s

; Half precision is not supported in hardware: an f16 load extends via a
; conversion libcall, and a store truncates via one.

; CHECK-LABEL: load_ext:
; CHECK:       ldq $27, __extendhfsf2($29){{.*}}!literal
; CHECK:       ret
define float @load_ext(ptr %p) {
  %h = load half, ptr %p
  %r = fpext half %h to float
  ret float %r
}

; CHECK-LABEL: store_trunc:
; CHECK:       ldq $27, __truncsfhf2($29){{.*}}!literal
; CHECK:       ret
define void @store_trunc(ptr %p, float %x) {
  %h = fptrunc float %x to half
  store half %h, ptr %p
  ret void
}

; A half that is only moved needs no conversion at all: it is a 16-bit datum, so
; the load and store are the ordinary unaligned word sequence and no libcall is
; emitted.  Nothing covered the no-conversion path.
; CHECK-LABEL: copy:
; CHECK-NOT:   __extendhfsf2
; CHECK-NOT:   __truncsfhf2
; CHECK-NOT:   jsr
; CHECK:       ldq_u
; CHECK:       ret
define void @copy(ptr %d, ptr %s) {
  %h = load half, ptr %s
  store half %h, ptr %d
  ret void
}
