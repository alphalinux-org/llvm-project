; RUN: llc -mtriple=alpha-unknown-linux-gnu -global-isel -global-isel-abort=1 -O2 < %s | FileCheck %s

; Every atomic on this target is an ldq_l/stq_c retry loop.  The loop is not
; built by the selector: the architecture requires that no memory access appear
; between the load locked and the store conditional, and a spill placed inside
; that window makes the store conditional fail every time round, so the
; selector emits a pseudo standing for the loop and AlphaExpandAtomicPseudo
; builds it after register allocation.  That is the same pseudo, and the same
; expansion, the SelectionDAG path reaches through a custom inserter.
;
; A quadword and a longword are locked directly; a byte and a word are read and
; written through the quadword they fall in, which is the longer form.

; CHECK-LABEL: rmw64:
; CHECK:       ldq_l $0, 0($16)
; CHECK-NEXT:  addq $0, $17, $[[T:[0-9]+]]
; CHECK-NEXT:  stq_c $[[T]], 0($16)
; CHECK-NEXT:  beq $[[T]], .LBB0_1
define i64 @rmw64(ptr %p, i64 %v) {
  %a = atomicrmw add ptr %p, i64 %v seq_cst
  ret i64 %a
}

; CHECK-LABEL: rmw32:
; CHECK:       ldl_l $0, 0($16)
; CHECK-NEXT:  subq $0, $17, $[[T:[0-9]+]]
; CHECK-NEXT:  stl_c $[[T]], 0($16)
define i32 @rmw32(ptr %p, i32 %v) {
  %a = atomicrmw sub ptr %p, i32 %v seq_cst
  ret i32 %a
}

; CHECK-LABEL: rmw16:
; CHECK:       bic $16, 7, $[[A:[0-9]+]]
; CHECK:       ldq_l $[[Q:[0-9]+]], 0($[[A]])
; CHECK:       extwl $[[Q]], $16, $[[V:[0-9]+]]
; CHECK:       and $[[V]], $17,
; CHECK:       mskwl $[[Q]], $16,
; CHECK:       inswl
; CHECK:       stq_c $[[Q]], 0($[[A]])
define i16 @rmw16(ptr %p, i16 %v) {
  %a = atomicrmw and ptr %p, i16 %v seq_cst
  ret i16 %a
}

; An exchange applies no operation to what it read; the value it writes is
; positioned once, outside the loop.
; CHECK-LABEL: rmw8:
; CHECK:       insbl $17, $16, $[[N:[0-9]+]]
; CHECK:       ldq_l $[[Q:[0-9]+]], 0($[[A:[0-9]+]])
; CHECK:       extbl $[[Q]], $16,
; CHECK:       mskbl $[[Q]], $16, $[[M:[0-9]+]]
; CHECK-NEXT:  bis $[[N]], $[[M]], $[[Q]]
; CHECK-NEXT:  stq_c $[[Q]], 0($[[A]])
define i8 @rmw8(ptr %p, i8 %v) {
  %a = atomicrmw xchg ptr %p, i8 %v seq_cst
  ret i8 %a
}

; The loop gives back the value it read and nothing else, so a compare-and-swap
; that wants the success flag gets it from comparing that against the
; comparand.  Leaving the loop early on a mismatch is what makes the store
; conditional a store only on the path that took it.
; CHECK-LABEL: cas64:
; CHECK:       ldq_l $0, 0($16)
; CHECK-NEXT:  cmpeq $0, $17, $[[E:[0-9]+]]
; CHECK-NEXT:  beq $[[E]], .LBB4_3
; CHECK:       stq_c ${{[0-9]+}}, 0($16)
define i64 @cas64(ptr %p, i64 %c, i64 %n) {
  %a = cmpxchg ptr %p, i64 %c, i64 %n seq_cst seq_cst
  %v = extractvalue {i64, i1} %a, 0
  ret i64 %v
}

; CHECK-LABEL: cas8:
; CHECK:       bic $16, 7, $[[A:[0-9]+]]
; CHECK:       ldq_l $[[Q:[0-9]+]], 0($[[A]])
; CHECK:       extbl $[[Q]], $16,
; CHECK:       cmpeq
; CHECK:       mskbl $[[Q]], $16,
; CHECK:       insbl $18, $16,
; CHECK:       stq_c $[[Q]], 0($[[A]])
define i1 @cas8(ptr %p, i8 %c, i8 %n) {
  %a = cmpxchg ptr %p, i8 %c, i8 %n seq_cst seq_cst
  %v = extractvalue {i8, i1} %a, 1
  ret i1 %v
}

; What AtomicExpand rewrites in the IR before either path sees it: there is no
; loop pseudo for a max, so it arrives as a compare-and-swap loop already.
; CHECK-LABEL: rmwmax:
; CHECK:       ldq_l
; CHECK:       cmpeq
; CHECK:       stq_c
define i64 @rmwmax(ptr %p, i64 %v) {
  %a = atomicrmw max ptr %p, i64 %v seq_cst
  ret i64 %a
}
