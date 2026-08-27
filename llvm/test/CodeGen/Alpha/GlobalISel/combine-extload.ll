; RUN: llc -mtriple=alpha-unknown-linux-gnu -mcpu=ev6 -global-isel -global-isel-abort=1 -O2 < %s | FileCheck %s
; RUN: llc -mtriple=alpha-unknown-linux-gnu -mcpu=ev6 -global-isel -global-isel-abort=1 -O2 \
; RUN:   -alphapostlegalizercombiner-disable-rule=load_and_mask < %s | FileCheck %s --check-prefix=NOCOMBINE

; Every narrow load on this target already gives back a zero-extended value --
; ldbu and ldwu are the only narrow loads there are -- so an extension after
; one is redundant.  Nothing in the legalizer can see that: it splits the
; zext into a load and a mask and stops there.  The post-legalizer combiner is
; what removes the mask, which is the point of having one.
;
; The second run line turns the rule off, so what the test measures is the
; combiner and not the rest of the pipeline.

; CHECK-LABEL: zl:
; CHECK:       ldbu $0, 0($16)
; CHECK-NEXT:  ret
; NOCOMBINE-LABEL: zl:
; NOCOMBINE:       ldbu $0, 0($16)
; NOCOMBINE-NEXT:  and $0, 255, $0
define i64 @zl(ptr %p) {
  %v = load i8, ptr %p
  %z = zext i8 %v to i64
  ret i64 %z
}

; CHECK-LABEL: zw:
; CHECK:       ldwu $0, 0($16)
; CHECK-NEXT:  ret
define i64 @zw(ptr %p) {
  %v = load i16, ptr %p
  %z = zext i16 %v to i64
  ret i64 %z
}

; A longword load sign-extends, so the mask is not redundant here and has to
; stay.
; CHECK-LABEL: zl32:
; CHECK:       ldl $0, 0($16)
; CHECK-NEXT:  zapnot $0, 15, $0
define i64 @zl32(ptr %p) {
  %v = load i32, ptr %p
  %z = zext i32 %v to i64
  ret i64 %z
}
