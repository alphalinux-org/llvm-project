; RUN: llc -mtriple=alpha-unknown-linux-gnu -stop-after=alpha-expand-atomic-pseudo \
; RUN:   -verify-machineinstrs < %s | FileCheck %s
; RUN: llc -mtriple=alpha-unknown-linux-gnu -mattr=+safe-bwa \
; RUN:   -stop-after=alpha-expand-atomic-pseudo -verify-machineinstrs < %s \
; RUN:   | FileCheck %s --check-prefix=BWA
; RUN: llc -mtriple=alpha-unknown-linux-gnu -mattr=+safe-partial \
; RUN:   -stop-after=alpha-expand-atomic-pseudo -verify-machineinstrs < %s \
; RUN:   | FileCheck %s --check-prefix=PARTIAL

; The ldq_l/stq_c loops are built by AlphaExpandAtomicPseudo rather than by a
; pattern, so the memory operand the pseudo carried has to be put onto the load
; and the store by hand.  Each gets the half of the access it performs -- an
; instruction that only loads may not be handed an operand claiming a store --
; and with it the ordering, the volatility and the object the access names.
; That pass runs after register allocation, so this stops after it rather than
; after selection, where the loop is still one instruction.

; CHECK-LABEL: name: rmw
; CHECK: LDQ_L {{.*}} :: (load monotonic (s64) from %ir.p)
; CHECK: STQ_C {{.*}} :: (store monotonic (s64) into %ir.p)
define i64 @rmw(ptr %p, i64 %v) {
  %r = atomicrmw add ptr %p, i64 %v seq_cst
  ret i64 %r
}

; CHECK-LABEL: name: cas
; CHECK: LDQ_L {{.*}} :: (load monotonic monotonic (s64) from %ir.p)
; CHECK: STQ_C {{.*}} :: (store monotonic monotonic (s64) into %ir.p)
define i64 @cas(ptr %p, i64 %c, i64 %n) {
  %r = cmpxchg ptr %p, i64 %c, i64 %n seq_cst seq_cst
  %v = extractvalue { i64, i1 } %r, 0
  ret i64 %v
}

; CHECK-LABEL: name: subword_rmw
; CHECK: LDQ_L {{.*}} :: (load monotonic (s8) from %ir.p)
; CHECK: STQ_C {{.*}} :: (store monotonic (s8) into %ir.p)
define i8 @subword_rmw(ptr %p, i8 %v) {
  %r = atomicrmw add ptr %p, i8 %v seq_cst
  ret i8 %r
}

; CHECK-LABEL: name: subword_cas
; CHECK: LDQ_L {{.*}} :: (load monotonic monotonic (s8) from %ir.p)
; CHECK: STQ_C {{.*}} :: (store monotonic monotonic (s8) into %ir.p)
define i8 @subword_cas(ptr %p, i8 %c, i8 %n) {
  %r = cmpxchg ptr %p, i8 %c, i8 %n seq_cst seq_cst
  %v = extractvalue { i8, i1 } %r, 0
  ret i8 %v
}

; -msafe-bwa turns a plain byte store into the same kind of loop, and the store
; being volatile has to survive into it.
; BWA-LABEL: name: safe_store
; BWA: LDQ_L {{.*}} :: (volatile load (s8) from %ir.p)
; BWA: STQ_C {{.*}} :: (volatile store (s8) into %ir.p)
define void @safe_store(ptr %p, i8 %v) {
  store volatile i8 %v, ptr %p
  ret void
}

; -msafe-partial does the same for a misaligned store, once per spanned
; quadword.
; PARTIAL-LABEL: name: partial_store
; PARTIAL: LDQ_L {{.*}} :: (volatile load (s32) from %ir.p, align 1)
; PARTIAL: STQ_C {{.*}} :: (volatile store (s32) into %ir.p, align 1)
; PARTIAL: LDQ_L {{.*}} :: (volatile load (s32) from %ir.p, align 1)
; PARTIAL: STQ_C {{.*}} :: (volatile store (s32) into %ir.p, align 1)
define void @partial_store(ptr %p, i32 %v) {
  store volatile i32 %v, ptr %p, align 1
  ret void
}
