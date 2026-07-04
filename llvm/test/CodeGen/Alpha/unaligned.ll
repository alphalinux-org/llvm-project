; RUN: llc -mtriple=alpha-unknown-linux-gnu -O2 < %s | FileCheck %s

; A misaligned word/long/quad access uses the ldq_u pair with extract (load) or
; insert/mask (store), rather than a byte-by-byte sequence.
;
; The second quadword is the one holding the datum's last byte, so its address
; is the first plus the width less one: 7, 3 and 1 here.  An offset of the full
; width would read a quadword past the end whenever the datum is aligned.

; CHECK-LABEL: load_q:
; CHECK:      ldq_u [[LO:\$[0-9]+]], 0($16)
; CHECK:      extql [[LO]], $16, [[LO]]
; CHECK:      addq $16, 7, [[HA:\$[0-9]+]]
; CHECK-NEXT: ldq_u [[HI:\$[0-9]+]], 0([[HA]])
; CHECK-NEXT: extqh [[HI]], $16, [[HI]]
; CHECK:      bis [[LO]], [[HI]], $0
; CHECK: ret
define i64 @load_q(ptr %p) {
  %v = load i64, ptr %p, align 1
  ret i64 %v
}

; CHECK-LABEL: store_q:
; CHECK-DAG: mskql
; CHECK-DAG: mskqh
; CHECK-DAG: insql
; CHECK-DAG: insqh
; CHECK-DAG: stq_u
; CHECK: ret
define void @store_q(ptr %p, i64 %v) {
  store i64 %v, ptr %p, align 1
  ret void
}

; CHECK-LABEL: load_l:
; CHECK:      extll
; CHECK:      addq $16, 3, [[HA:\$[0-9]+]]
; CHECK-NEXT: ldq_u [[HI:\$[0-9]+]], 0([[HA]])
; CHECK-NEXT: extlh [[HI]], $16,
define i32 @load_l(ptr %p) {
  %v = load i32, ptr %p, align 1
  ret i32 %v
}

; CHECK-LABEL: load_w:
; CHECK:      extwl
; CHECK:      addq $16, 1, [[HA:\$[0-9]+]]
; CHECK-NEXT: ldq_u [[HI:\$[0-9]+]], 0([[HA]])
; CHECK-NEXT: extwh [[HI]], $16,
define i16 @load_w(ptr %p) {
  %v = load i16, ptr %p, align 1
  ret i16 %v
}

; An aligned access stays a single ldq/stq.
; CHECK-LABEL: aligned:
; CHECK: ldq $0, 0($16)
; CHECK-NOT: ldq_u
define i64 @aligned(ptr %p) {
  %v = load i64, ptr %p, align 8
  ret i64 %v
}
