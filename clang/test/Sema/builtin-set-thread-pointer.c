// RUN: %clang_cc1 -triple alpha-unknown-linux-gnu -fsyntax-only -verify=alpha %s
// RUN: %clang_cc1 -triple x86_64-unknown-linux-gnu -fsyntax-only -verify=other %s
// RUN: %clang_cc1 -triple aarch64-unknown-linux-gnu -fsyntax-only -verify=other %s

// __builtin_set_thread_pointer is an Alpha target builtin.  On any other
// target it must be diagnosed rather than reaching CodeGen, which would emit
// an Alpha intrinsic that the target's back end cannot select.

void f(void *p) {
  // alpha-no-diagnostics
  __builtin_set_thread_pointer(p); // other-error {{use of unknown builtin}}
}
