; RUN: llc -mtriple=alpha-unknown-linux-gnu -global-isel -global-isel-abort=1 \
; RUN:     < %s | FileCheck %s
;
; SelectionDAG remains the default path; this is what GlobalISel selects when
; it is asked for, with no fallback allowed.

; CHECK-LABEL: nothing:
; CHECK:       ret
define void @nothing() {
  ret void
}

; CHECK-LABEL: arith:
; CHECK:      mulq $16, $17, $0
; CHECK-NEXT: subq $0, $16, $0
; CHECK-NEXT: xor $0, $17, $0
; CHECK-NEXT: ret
define i64 @arith(i64 %a, i64 %b) {
  %m = mul i64 %a, %b
  %s = sub i64 %m, %a
  %x = xor i64 %s, %b
  ret i64 %x
}

; CHECK-LABEL: constant:
; CHECK:      lda $0, 42($31)
; CHECK-NEXT: ret
define i64 @constant() {
  ret i64 42
}

; A constant offset folds into the memory operand.
; CHECK-LABEL: memory:
; CHECK:      ldq $0, 24($16)
; CHECK-NEXT: stq $0, 24($16)
; CHECK-NEXT: ret
define i64 @memory(ptr %p) {
  %q = getelementptr i64, ptr %p, i64 3
  %v = load i64, ptr %q
  store i64 %v, ptr %q
  ret i64 %v
}

; CHECK-LABEL: fpmath:
; CHECK:      mult $f16, $f17, $f0
; CHECK-NEXT: addt $f0, $f16, $f0
; CHECK-NEXT: ret
define double @fpmath(double %a, double %b) {
  %m = fmul double %a, %b
  %s = fadd double %m, %a
  ret double %s
}

; There is no greater-than compare: the operands are swapped instead, and
; inequality is equality inverted.
; CHECK-LABEL: compares:
; CHECK-DAG:  cmplt $17, $16, {{\$[0-9]+}}
; CHECK-DAG:  cmpeq $16, $17, {{\$[0-9]+}}
; CHECK-DAG:  xor {{\$[0-9]+}}, 1, {{\$[0-9]+}}
; CHECK:      ret
define i64 @compares(i64 %a, i64 %b) {
  %gt = icmp sgt i64 %a, %b
  %ne = icmp ne i64 %a, %b
  %x = and i1 %gt, %ne
  %z = zext i1 %x to i64
  ret i64 %z
}

; The condition is tested against zero; which way the branch goes depends on
; how the blocks are laid out.
; CHECK-LABEL: branches:
; CHECK:      cmplt
; CHECK:      b{{eq|ne}} {{\$[0-9]+}}, {{\.LBB[0-9_]+}}
define i64 @branches(i64 %n) {
entry:
  %c = icmp sgt i64 %n, 0
  br i1 %c, label %then, label %else
then:
  ret i64 1
else:
  ret i64 0
}

declare i64 @callee(i64, i64)

; CHECK-LABEL: docall:
; CHECK:      ldgp $29, 0($27)
; CHECK:      lda $17, 7($31)
; CHECK:      ldq $27, callee($29)
; CHECK:      jsr $26, ($27)
; CHECK:      ldgp $29, 0($26)
define i64 @docall(i64 %a) {
  %r = call i64 @callee(i64 %a, i64 7)
  ret i64 %r
}

@g = external global i64
@arr = external global [4 x i8]

; The address of a global is read out of the GOT.
; CHECK-LABEL: globals:
; CHECK:      ldgp $29, 0($27)
; CHECK:      ldq {{\$[0-9]+}}, g($29){{.*}}!literal
; CHECK:      ldq {{\$[0-9]+}}, 0({{\$[0-9]+}})
define i64 @globals() {
  %v = load i64, ptr @g
  ret i64 %v
}

; Without BWX a byte load reads the quadword holding it and extracts the byte,
; and a byte store is the read-modify-write pseudo.
; CHECK-LABEL: narrow:
; CHECK:      ldq_u {{\$[0-9]+}}, 0({{\$[0-9]+}})
; CHECK:      extbl
; CHECK:      ldq_u
; CHECK:      mskbl
; CHECK:      insbl
; CHECK:      stq_u
define void @narrow(ptr %p, ptr %q) {
  %v = load i8, ptr %p
  store i8 %v, ptr %q
  ret void
}

; A float constant lives in the constant pool, addressed from the global
; pointer.
; CHECK-LABEL: fconst:
; CHECK:      ldah {{\$[0-9]+}}, .LCPI{{[0-9_]+}}($29){{.*}}!gprelhigh
; CHECK:      ldt {{\$f[0-9]+}}, .LCPI{{[0-9_]+}}({{\$[0-9]+}}){{.*}}!gprellow
; CHECK:      divt
define double @fconst(double %a) {
  %r = fdiv double %a, 2.0
  ret double %r
}

; The shapes the GCC torture suite exercised when the two paths were compared:
; a loop with a variable trip count, an array indexed through a pointer, and a
; variadic call.
declare i32 @printf(ptr, ...)
@fmt = private unnamed_addr constant [4 x i8] c"%ld\00"

; CHECK-LABEL: sumloop:
; CHECK:      ldq {{\$[0-9]+}}, 0({{\$[0-9]+}})
; CHECK:      addq
; CHECK:      ret
define i64 @sumloop(ptr %p, i64 %n) {
entry:
  br label %loop
loop:
  %i = phi i64 [ 0, %entry ], [ %i.next, %loop ]
  %acc = phi i64 [ 0, %entry ], [ %acc.next, %loop ]
  %addr = getelementptr i64, ptr %p, i64 %i
  %v = load i64, ptr %addr
  %acc.next = add i64 %acc, %v
  %i.next = add i64 %i, 1
  %done = icmp eq i64 %i.next, %n
  br i1 %done, label %exit, label %loop
exit:
  ret i64 %acc.next
}

; A variadic call passes each argument in the register its type selects; the
; callee saves both banks, so nothing has to be duplicated.
; CHECK-LABEL: variadic:
; CHECK:      ldq $27, printf($29)
; CHECK:      jsr $26, ($27)
define void @variadic(i64 %v) {
  %c = call i32 (ptr, ...) @printf(ptr @fmt, i64 %v)
  ret void
}
