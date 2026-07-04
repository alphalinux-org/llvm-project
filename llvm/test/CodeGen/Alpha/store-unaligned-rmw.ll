; RUN: llc -mtriple=alpha-unknown-linux-gnu < %s | FileCheck %s
; RUN: llc -mtriple=alpha-unknown-linux-gnu -stop-after=finalize-isel < %s \
; RUN:   | FileCheck %s --check-prefix=MIR
; RUN: llc -mtriple=alpha-unknown-linux-gnu -stop-after=postrapseudos \
; RUN:   -verify-machineinstrs < %s | FileCheck %s --check-prefix=EXPANDED

; The node has to be built as a memory node: selection reads the memory operand
; back off it to give to the instruction, so building it as a plain node left
; the instruction carrying whatever followed the node in memory.  It reads the
; one or two quadwords the field falls in before writing them back, so the
; operand describes a load as well as a store and covers both of them.
; MIR: RMW_USTORE {{.*}} :: (load store (s128), align 8)
; MIR: RMW_USTORE {{.*}} :: (load store (s128), align 8)

; A misaligned store reads the quadwords its field falls in, splices the field
; in and writes them back.  Two such stores can fall in one quadword, so the
; reads of one must not be hoisted above the write-backs of the other -- which
; is exactly what the DAG scheduler does with the pieces written out
; separately.  Being one instruction until register allocation is not enough on
; its own either, so the expansion leaves a bundle behind.

; CHECK-LABEL: two_stores:
; CHECK:      ldq_u [[A:\$[0-9]+]], 0([[P:\$[0-9]+]])
; CHECK:      stq_u {{\$[0-9]+}}, 0({{\$[0-9]+}})
; CHECK:      stq_u [[A]], 0([[P]])
; CHECK:      ldq_u
; CHECK:      stq_u
; CHECK:      stq_u
;
; EXPANDED-LABEL: name: two_stores
; EXPANDED:      BUNDLE
; EXPANDED-SAME: {
; EXPANDED:        LDQ_U {{.*}} :: (load (s128), align 8)
; EXPANDED-NOT:  }
; EXPANDED:        STQ_U {{.*}} :: (store (s128), align 8)
; EXPANDED:      }
define void @two_stores(ptr %p, ptr %q, i64 %a, i64 %b) {
  store i64 %a, ptr %p, align 1
  store i64 %b, ptr %q, align 1
  ret void
}

; A volatile misaligned store must still be volatile once it is expanded --
; dropping the memory operands would lose that, and with it the guarantee that
; the access is not moved or duplicated.
; EXPANDED-LABEL: name: volatile_store
; EXPANDED:      BUNDLE
; EXPANDED:        LDQ_U {{.*}} :: (volatile load (s128), align 8)
; EXPANDED:        STQ_U {{.*}} :: (volatile store (s128), align 8)
define void @volatile_store(ptr %p, i64 %a) {
  store volatile i64 %a, ptr %p, align 1
  ret void
}
