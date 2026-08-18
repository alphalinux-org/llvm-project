; RUN: llc -mtriple=alpha-unknown-linux-gnu -O0 < %s | FileCheck %s \
; RUN:   --implicit-check-not={{__extend..tf2}} \
; RUN:   --implicit-check-not={{__trunctf.f2}}
; RUN: llc -mtriple=alpha-unknown-linux-gnu -O2 < %s | FileCheck %s \
; RUN:   --implicit-check-not={{__extend..tf2}} \
; RUN:   --implicit-check-not={{__trunctf.f2}}
;
; A conversion next to a memory access must not be folded into it: an f128
; extending load or truncating store hides the conversion inside a node the
; OTS interception never sees, and softening that emits calls to __extendsftf2
; and friends, which nothing on Alpha provides.

; __extendsftf2 and __trunctfdf2 and their siblings are what softening emits
; when a conversion does get folded into the access, so no output may name one
; anywhere -- which is what the implicit-check-nots on the RUN lines say.  A
; CHECK-NOT written here instead would only span as far as the first label.

; CHECK-LABEL: ext_from_load_f32:
; CHECK: _OtsConvertFloatTX
define void @ext_from_load_f32(ptr %ret, ptr %p) {
  %a = load float, ptr %p
  %r = fpext float %a to fp128
  store fp128 %r, ptr %ret
  ret void
}

; CHECK-LABEL: ext_from_load_f16:
; CHECK: _OtsConvertFloatTX
define void @ext_from_load_f16(ptr %ret, ptr %p) {
  %a = load half, ptr %p
  %r = fpext half %a to fp128
  store fp128 %r, ptr %ret
  ret void
}

; CHECK-LABEL: round_to_store_f64:
; CHECK: _OtsConvertFloatXT
define void @round_to_store_f64(ptr %p, ptr %q) {
  %a = load fp128, ptr %q
  %r = fptrunc fp128 %a to double
  store double %r, ptr %p
  ret void
}

; The f128 -> f32 sequence ends in an f64 -> f32 round, which must not fold
; into the store either: Alpha has no truncating float store.
; CHECK-LABEL: round_to_store_f32:
; CHECK: _OtsConvertFloatXT
; CHECK: cvtts
define void @round_to_store_f32(ptr %p, ptr %q) {
  %a = load fp128, ptr %q
  %r = fptrunc fp128 %a to float
  store float %r, ptr %p
  ret void
}
