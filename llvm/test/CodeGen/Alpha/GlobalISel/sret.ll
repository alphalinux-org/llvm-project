; RUN: llc -mtriple=alpha-unknown-linux-gnu -global-isel -global-isel-abort=1 < %s | FileCheck %s
; RUN: llc -mtriple=alpha-unknown-linux-gnu < %s | FileCheck %s

; A function returning in memory hands the buffer pointer it was given back in
; $0.  The calling convention tables say nothing about this, so both paths do it
; by hand and this test is what keeps them agreeing.

%struct.big = type { i64, i64, i64 }

; CHECK-LABEL: fill:
; CHECK:       bis $31, $16, $0
define void @fill(ptr sret(%struct.big) %ret, i64 %x) {
  store i64 %x, ptr %ret
  ret void
}

; A value the return convention cannot assign -- an aggregate, a vector,
; anything wider than a quadword -- is demoted to that same convention: the
; caller passes a buffer in $16 ahead of the declared arguments, the callee
; fills it and hands the pointer back in $0.  GlobalISel used to fail to
; translate such a return at all and hand the whole function to the
; SelectionDAG path, so these check both paths against each other.

; The declared arguments move along by one register to make room for the
; buffer: %x arrives in $17, not $16.
; CHECK-LABEL: mk:
; CHECK:       bis $31, $16, $0
; CHECK-DAG:   stq $17, 0($0)
; CHECK-DAG:   stq $18, 8($0)
define %struct.big @mk(i64 %x, i64 %y) {
  %r0 = insertvalue %struct.big poison, i64 %x, 0
  %r1 = insertvalue %struct.big %r0, i64 %y, 1
  %r2 = insertvalue %struct.big %r1, i64 42, 2
  ret %struct.big %r2
}

; CHECK-LABEL: wide:
; CHECK:       bis $31, $16, $0
; CHECK:       cmpult
; CHECK-DAG:   stq ${{[0-9]+}}, 0($0)
; CHECK-DAG:   stq ${{[0-9]+}}, 8($0)
define i128 @wide(i128 %x, i128 %y) {
  %r = add i128 %x, %y
  ret i128 %r
}

; The caller side: it provides the buffer as a stack object, hands the address
; over in $16, and reads the pieces back out of it afterwards.
; CHECK-LABEL: use:
; CHECK:       jsr $26, ($27), mk
; CHECK:       ldq
; CHECK:       addq
define i64 @use(i64 %x) {
  %s = call %struct.big @mk(i64 %x, i64 7)
  %a = extractvalue %struct.big %s, 0
  %b = extractvalue %struct.big %s, 2
  %c = add i64 %a, %b
  ret i64 %c
}

; A vector is demoted by the same rule, which is what makes one usable as a
; return type at all: there are no vector registers.
; CHECK-LABEL: vec:
; CHECK:       bis $31, $16, $0
define <4 x i32> @vec(<4 x i32> %a) {
  ret <4 x i32> %a
}
