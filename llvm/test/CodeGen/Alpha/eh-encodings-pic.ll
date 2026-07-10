; RUN: llc -mtriple=alpha-unknown-linux-gnu -relocation-model=pic < %s | FileCheck %s

; In PIC the personality routine and the LSDA are named by PC-relative
; encodings, so the unwind tables need no dynamic relocation: 0x9b is
; indirect|pcrel|sdata4 for the personality, 0x1b is pcrel|sdata4 for the LSDA.

; CHECK: .cfi_personality 155, DW.ref.__gxx_personality_v0
; CHECK: .cfi_lsda 27,

declare void @f()
declare i32 @__gxx_personality_v0(...)

define void @g() personality ptr @__gxx_personality_v0 {
entry:
  invoke void @f() to label %done unwind label %lpad
lpad:
  %e = landingpad { ptr, i32 } cleanup
  resume { ptr, i32 } %e
done:
  ret void
}
