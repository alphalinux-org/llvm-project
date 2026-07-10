// RUN: %clang_cc1 -triple alpha-unknown-linux-gnu -fsyntax-only -verify %s

// _BitInt works up to 64 bits: the backend promotes every sub-i64 integer to
// i64 without special-casing.  Wider is rejected, since there is no 128-bit
// integer instruction, and __BITINT_MAXWIDTH__ says so.

_Static_assert(__BITINT_MAXWIDTH__ == 64, "Alpha supports _BitInt up to 64 bits");

unsigned _BitInt(64) ok(unsigned _BitInt(64) x) { return x + 1; }
_BitInt(17) narrow(_BitInt(17) x) { return x + 1; }

unsigned _BitInt(128) too_wide(void); // expected-error {{unsigned _BitInt of bit sizes greater than 64 not supported}}
