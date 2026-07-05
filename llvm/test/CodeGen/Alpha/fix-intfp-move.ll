; RUN: llc -mtriple=alpha-unknown-linux-gnu < %s | FileCheck %s --check-prefix=BASE
; RUN: llc -mtriple=alpha-unknown-linux-gnu -mattr=+fix < %s | FileCheck %s --check-prefix=FIX

; Without the FIX extension an integer/FP bit move bounces through the stack;
; with FIX it is a single itoft/ftoit register move.

define double @i2f(i64 %x) {
; BASE-LABEL: i2f:
; BASE: stq $16,
; BASE: ldt $f0,
; FIX-LABEL: i2f:
; FIX: itoft $16, $f0
; FIX-NOT: ldt
  %r = bitcast i64 %x to double
  ret double %r
}

define i64 @f2i(double %x) {
; BASE-LABEL: f2i:
; BASE: stt $f16,
; BASE: ldq $0,
; FIX-LABEL: f2i:
; FIX: ftoit $f16, $0
; FIX-NOT: ldq
  %r = bitcast double %x to i64
  ret i64 %r
}

define float @i2f_s(i32 %x) {
; BASE-LABEL: i2f_s:
; BASE: stl $16,
; BASE: lds $f0,
; FIX-LABEL: i2f_s:
; FIX: itofs $16, $f0
; FIX-NOT: lds
  %r = bitcast i32 %x to float
  ret float %r
}

define i32 @f2i_s(float %x) {
; BASE-LABEL: f2i_s:
; BASE: sts $f16,
; BASE: ldl $0,
; FIX-LABEL: f2i_s:
; FIX: ftois $f16, $0
; FIX-NOT: ldl
  %r = bitcast float %x to i32
  ret i32 %r
}
