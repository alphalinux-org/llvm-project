// RUN: %clang_cc1 -fsyntax-only -verify %s
// expected-no-diagnostics

// GCC's noclone attribute prevents function cloning.  Clang does not clone
// functions, so it is accepted and ignored rather than warned about: glibc
// builds with -Werror and uses it (tst-stderr-compat.c).
__attribute__((noclone)) void f(void) {}

__attribute__((noclone)) __attribute__((noinline)) int g(int x) { return x; }

__attribute__((noclone)) static int h(void);
__attribute__((noclone)) static int h(void) { return 0; }
