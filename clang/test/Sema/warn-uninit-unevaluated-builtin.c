// RUN: %clang_cc1 -fsyntax-only -Wuninitialized -verify %s
// expected-no-diagnostics

// Builtins carrying the UnevaluatedArguments attribute never read their
// argument's value, only its type, so passing an uninitialized variable to one
// is not a use and must not warn.  The CFG builder omits the argument nodes.

int classify(void) {
  int x;
  return __builtin_classify_type(x);
}

int is_const(void) {
  int x;
  return __builtin_constant_p(x);
}

unsigned long object_size(void) {
  char *p;
  return __builtin_object_size(p, 0);
}

unsigned long dynamic_object_size(void) {
  char *p;
  return __builtin_dynamic_object_size(p, 0);
}
