// RUN: %clang_cc1 -fsyntax-only -Wuninitialized -verify %s

// Builtins carrying the UnevaluatedArguments attribute never read their
// argument's value, only whether it is a constant or what type it has, so
// passing an uninitialized variable to one is not a use and must not warn.
// OmitArguments takes the argument sub-expressions out of the CFG entirely,
// which is what makes the dataflow analyses stop seeing them.

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

// The remaining builtins with UnevaluatedArguments.  __builtin_os_log_format
// is the one to watch: its arguments really are evaluated, but by the paired
// __builtin_os_log_format call rather than by the size query.
unsigned long os_log_size(void) {
  int x;
  return __builtin_os_log_format_buffer_size("%d", x);
}

unsigned long alloc_token(void) {
  int x;
  return __builtin_infer_alloc_token(x);
}

// The control: an ordinary call does read its argument, so the warning has to
// fire here.  Without it every function above would pass with -Wuninitialized
// switched off entirely.
void sink(int);
void ordinary_call(void) {
  int x; // expected-note {{initialize the variable 'x' to silence this warning}}
  sink(x); // expected-warning {{variable 'x' is uninitialized when used here}}
}
