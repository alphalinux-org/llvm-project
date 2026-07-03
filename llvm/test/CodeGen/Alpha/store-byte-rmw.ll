; RUN: llc -mtriple=alpha-unknown-linux-gnu < %s | FileCheck %s
; RUN: llc -mtriple=alpha-unknown-linux-gnu -mcpu=ev56 < %s | FileCheck %s --check-prefix=BWX
; RUN: llc -mtriple=alpha-unknown-linux-gnu -stop-after=postrapseudos \
; RUN:   -verify-machineinstrs < %s | FileCheck %s --check-prefix=EXPANDED

; Without BWX a byte store reads the quadword holding the byte, splices the byte
; in and writes the quadword back.  Two such stores can target the same quadword
; and do not alias each other, so the scheduler is free to interleave them; each
; read-modify-write must stay together or the first byte written is lost when
; the second quadword write-back lands.
;
; The expansion runs before the post-RA scheduler rather than after it, so the
; pieces are held together in a bundle; what matters is that no second ldq_u
; appears between a read and its own write-back -- that interleaving is the bug.  The mask and the insert are
; independent of each other, so their order within the group is the scheduler's
; to choose and is checked with CHECK-DAG rather than pinned.
;
; The stored values are arguments rather than constants so that the two stores
; are not merged into a single word store, which is what they would become if
; both values were known.

%struct.S = type { i32, i8, i8, i8, i8 }

define void @two_bytes(ptr %p, i8 %x, i8 %y) {
; CHECK-LABEL: two_bytes:
; CHECK:      ldq_u [[T:\$[0-9]+]], 0([[A:\$[0-9]+]])
; CHECK-DAG:  mskbl [[T]], [[A]], [[T]]
; CHECK-DAG:  insbl {{\$[0-9]+}}, [[A]], [[U:\$[0-9]+]]
; CHECK-NOT:  ldq_u
; CHECK:      bis [[T]], [[U]], [[T]]
; CHECK-NEXT: stq_u [[T]], 0([[A]])
; CHECK:      ldq_u [[T2:\$[0-9]+]], 0([[A2:\$[0-9]+]])
; CHECK-DAG:  mskbl [[T2]], [[A2]], [[T2]]
; CHECK-DAG:  insbl {{\$[0-9]+}}, [[A2]], [[U2:\$[0-9]+]]
; CHECK-NOT:  ldq_u
; CHECK:      bis [[T2]], [[U2]], [[T2]]
; CHECK-NEXT: stq_u [[T2]], 0([[A2]])
;
; A CPU with BWX just stores the bytes.
; BWX-LABEL: two_bytes:
; BWX-NOT:   ldq_u
; BWX:       stb
; BWX-NOT:   ldq_u
  %a = getelementptr inbounds %struct.S, ptr %p, i32 0, i32 1
  store i8 %x, ptr %a, align 4
  %b = getelementptr inbounds %struct.S, ptr %p, i32 0, i32 2
  store i8 %y, ptr %b, align 1
  ret void
}

define void @word(ptr %p) {
; CHECK-LABEL: word:
; CHECK:      ldq_u [[T:\$[0-9]+]], 0([[A:\$[0-9]+]])
; CHECK-DAG:  mskwl [[T]], [[A]], [[T]]
; CHECK-DAG:  inswl {{\$[0-9]+}}, [[A]], [[U:\$[0-9]+]]
; CHECK-NOT:  ldq_u
; CHECK:      bis [[T]], [[U]], [[T]]
; CHECK-NEXT: stq_u [[T]], 0([[A]])
  store i16 1, ptr %p, align 2
  ret void
}

; The read-modify-write is one bundle, and it keeps the memory operands the
; pseudo carried: each piece gets the half of the access it performs, and a
; volatile store stays volatile.
define void @volatile_byte(ptr %p, i8 %x) {
; EXPANDED-LABEL: name: volatile_byte
; EXPANDED:      BUNDLE
; EXPANDED-NEXT:   LDQ_U {{.*}} :: (volatile load (s64))
; EXPANDED-NEXT:   MSKBL
; EXPANDED-NEXT:   INSBL
; EXPANDED-NEXT:   BIS
; EXPANDED-NEXT:   STQ_U {{.*}} :: (volatile store (s64))
; EXPANDED-NEXT: }
  store volatile i8 %x, ptr %p
  ret void
}
