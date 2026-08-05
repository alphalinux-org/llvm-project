; RUN: llc -mtriple=alpha-unknown-linux-gnu < %s | FileCheck %s

; Only $0 carries a returned integer, so anything wider comes back in memory
; through a buffer the caller passes in $16, which is also what GCC does.  A
; complex float or double is the exception: its two halves ride in $f0/$f1.

; CHECK-LABEL: ret_i128:
; CHECK-DAG: bis $31, $16, $0
; CHECK-DAG: stq $17, 0($0)
; CHECK-DAG: stq $18, 8($0)
define i128 @ret_i128(i128 %x) {
  ret i128 %x
}

; The same convention reaches the runtime calls the back end itself creates: the
; buffer is handed to __ashlti3 in $16 and the result read back out of it.
; CHECK-LABEL: call_libcall:
; CHECK: lda $16, {{[0-9]+}}($30)
; CHECK: ldq $27, __ashlti3($29)
define i128 @call_libcall(i128 %x, i128 %s) {
  %r = shl i128 %x, %s
  ret i128 %r
}

; CHECK-LABEL: ret_two_doubles:
; CHECK-DAG: cpys $f16, $f16, $f0
; CHECK-DAG: cpys $f17, $f17, $f1
define { double, double } @ret_two_doubles(double %a, double %b) {
  %r0 = insertvalue { double, double } poison, double %a, 0
  %r1 = insertvalue { double, double } %r0, double %b, 1
  ret { double, double } %r1
}
