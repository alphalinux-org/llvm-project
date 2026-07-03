; RUN: llc -mtriple=alpha-unknown-linux-gnu -mcpu=ev6 -stop-after=virtregrewriter \
; RUN:   -verify-machineinstrs < %s | FileCheck %s

; A spill and its reload each carry a memory operand naming the stack slot.
; Without one the access describes nothing, so anything that asks what an
; instruction touches -- alias analysis, the scheduler's memory dependences --
; has to assume the worst of every spill in the function, and two spills to
; different slots look as though they might be the same store.

declare void @clobber()

; CHECK-LABEL: name: spill_fp
; CHECK-DAG: STT killed $f{{[0-9]+}}, %stack.[[A:[0-9]+]], 0 :: (store (s64) into %stack.[[A]])
; CHECK-DAG: $f{{[0-9]+}} = LDT %stack.[[A]], 0 :: (load (s64) from %stack.[[A]])
define void @spill_fp(ptr %p) {
  %a = load volatile double, ptr %p
  %b = load volatile double, ptr %p
  %c = load volatile double, ptr %p
  %d = load volatile double, ptr %p
  %e = load volatile double, ptr %p
  %f = load volatile double, ptr %p
  %g = load volatile double, ptr %p
  %h = load volatile double, ptr %p
  %i = load volatile double, ptr %p
  %j = load volatile double, ptr %p
  %k = load volatile double, ptr %p
  %l = load volatile double, ptr %p
  call void @clobber()
  store volatile double %a, ptr %p
  store volatile double %b, ptr %p
  store volatile double %c, ptr %p
  store volatile double %d, ptr %p
  store volatile double %e, ptr %p
  store volatile double %f, ptr %p
  store volatile double %g, ptr %p
  store volatile double %h, ptr %p
  store volatile double %i, ptr %p
  store volatile double %j, ptr %p
  store volatile double %k, ptr %p
  store volatile double %l, ptr %p
  ret void
}

; CHECK-LABEL: name: spill_gpr
; CHECK-DAG: STQ killed $r{{[0-9]+}}, %stack.[[B:[0-9]+]], 0 :: (store (s64) into %stack.[[B]])
; CHECK-DAG: $r{{[0-9]+}} = LDQ %stack.[[B]], 0 :: (load (s64) from %stack.[[B]])
define void @spill_gpr(ptr %p) {
  %a = load volatile i64, ptr %p
  %b = load volatile i64, ptr %p
  %c = load volatile i64, ptr %p
  %d = load volatile i64, ptr %p
  %e = load volatile i64, ptr %p
  %f = load volatile i64, ptr %p
  %g = load volatile i64, ptr %p
  %h = load volatile i64, ptr %p
  %i = load volatile i64, ptr %p
  %j = load volatile i64, ptr %p
  %k = load volatile i64, ptr %p
  %l = load volatile i64, ptr %p
  call void @clobber()
  store volatile i64 %a, ptr %p
  store volatile i64 %b, ptr %p
  store volatile i64 %c, ptr %p
  store volatile i64 %d, ptr %p
  store volatile i64 %e, ptr %p
  store volatile i64 %f, ptr %p
  store volatile i64 %g, ptr %p
  store volatile i64 %h, ptr %p
  store volatile i64 %i, ptr %p
  store volatile i64 %j, ptr %p
  store volatile i64 %k, ptr %p
  store volatile i64 %l, ptr %p
  ret void
}
