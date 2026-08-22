// RUN: %clang_cc1 -triple alpha-unknown-linux-gnu -fsyntax-only -verify %s

// The letters accepted here are the ones the back end lowers.  GCC also has
// 'G' for the floating-point zero, 'W' for the vector zero, 'T' for a
// high-part symbol and 'w' for a memory whose address is a bare register;
// accepting those would only move the failure to a back end with no case for
// them.  Letters GCC's alpha port does not define at all, such as 'H' and
// 'U', are rejected for the same reason GCC rejects them.

void ranges(void) {
  static const int BelowMin = -1;
  static const int AboveMax = 256;
  asm volatile("" ::"I"(BelowMin)); // expected-error{{value '-1' out of range for constraint 'I'}}
  asm volatile("" ::"I"(AboveMax)); // expected-error{{value '256' out of range for constraint 'I'}}
  asm volatile("" ::"I"(255));
  asm volatile("" ::"J"(1)); // expected-error{{value '1' out of range for constraint 'J'}}
  asm volatile("" ::"J"(0));
  asm volatile("" ::"K"(32768)); // expected-error{{value '32768' out of range for constraint 'K'}}
  asm volatile("" ::"K"(-32768));
  asm volatile("" ::"P"(4)); // expected-error{{value '4' out of range for constraint 'P'}}
  asm volatile("" ::"P"(3));
  asm volatile("" ::"S"(64)); // expected-error{{value '64' out of range for constraint 'S'}}
  asm volatile("" ::"S"(63));
}

void unsupported(double d, int i, int *p) {
  asm volatile("" ::"G"(0.0)); // expected-error{{invalid input constraint 'G' in asm}}
  asm volatile("" ::"H"(d));   // expected-error{{invalid input constraint 'H' in asm}}
  asm volatile("" ::"T"(i));   // expected-error{{invalid input constraint 'T' in asm}}
  asm volatile("" ::"W"(i));   // expected-error{{invalid input constraint 'W' in asm}}
  asm volatile("" ::"w"(*p));  // expected-error{{invalid input constraint 'w' in asm}}
  asm volatile("" ::"U"(*p));  // expected-error{{invalid input constraint 'U' in asm}}
}

void accepted(int i, int *p) {
  asm volatile("" ::"L"(0x10000));
  asm volatile("" ::"M"(0xff00ff00));
  asm volatile("" ::"N"(-256));
  asm volatile("" ::"O"(-255));
  asm volatile("" ::"f"(1.0));
  asm volatile("" ::"a"(i), "b"(i), "c"(i), "v"(i));
  asm volatile("" ::"Q"(*p));
}
