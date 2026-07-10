// RUN: %clang_cc1 -fsyntax-only -Wunknown-attributes -verify %s

// GCC's noclone attribute asks that a function not be cloned.  Clang has no
// way to honour that, so it is accepted and dropped rather than warned about:
// glibc builds with -Werror and uses it (tst-stderr-compat.c).
__attribute__((noclone)) void f(void) {}

// A redeclaration is the one case that could behave differently.
__attribute__((noclone)) static int h(void);
__attribute__((noclone)) static int h(void) { return 0; }

// IgnoredAttr sets no subjects and checks no arguments, so neither of these is
// diagnosed either.  That is the whole family's behaviour, not this attribute's
// -- pinned here so a change to it is a deliberate one.
__attribute__((noclone)) int v;
__attribute__((noclone(1))) void args(void) {}

// The control: an attribute that really is unknown still warns, so the silence
// above is this attribute being accepted rather than the warning being off.
__attribute__((noclon)) void typo(void) {} // expected-warning {{unknown attribute 'noclon' ignored}}
