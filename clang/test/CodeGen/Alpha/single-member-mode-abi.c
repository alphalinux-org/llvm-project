// RUN: %clang_cc1 -triple alpha-unknown-linux-gnu -emit-llvm -o - %s \
// RUN:   | FileCheck %s

// gcc gives a struct whose layout is one member the *mode* of that member and
// then passes the struct the way that mode is passed, so `struct { long double
// x; }` goes by invisible reference exactly as a bare long double does.  The
// rule recurses, so a nested struct and a one-element array reach it too, and
// it is a struct rule: a union with one member is still passed by value.
//
// It only shows up for the two modes that go by reference -- TFmode always,
// and SFmode/SCmode in a variadic tail -- because an aggregate of any other
// mode occupies the same integer slots either way.
//
// The rule is gcc's compute_record_mode (gcc/stor-layout.cc): it walks over a
// member of zero size rather than counting it, treats a C++ base as a member,
// and -- Alpha being STRICT_ALIGNMENT -- leaves the record in BLKmode when the
// record is aligned below the mode it would take.

struct sld  { long double x; };
struct sld1 { long double x[1]; };
struct sldn { struct sld i; };
struct sld2 { long double x[2]; };
union  uld  { long double ld; long l[2]; };
struct sldp { long double x; int pad; };

struct sf   { float f; };
struct sf1  { float f[1]; };
struct sfn  { struct sf i; };
struct sf2  { float a, b; };
union  uf   { float f; int i; };
union  uf1  { float f; };

struct sscf  { _Complex float c; };
struct sscfn { struct sscf i; };
union  uscf  { _Complex float c; };

struct sd   { double d; };
struct sscd { _Complex double c; };

// A single long double member: by reference in every position, named included.
// CHECK-LABEL: define dso_local void @named_sld(
// CHECK-SAME: ptr noundef byval(%struct.sld)
void named_sld(struct sld x) { (void)x; }

// CHECK-LABEL: define dso_local void @named_sld1(
// CHECK-SAME: ptr noundef byval(%struct.sld1)
void named_sld1(struct sld1 x) { (void)x; }

// CHECK-LABEL: define dso_local void @named_sldn(
// CHECK-SAME: ptr noundef byval(%struct.sldn)
void named_sldn(struct sldn x) { (void)x; }

// Two elements, so the struct keeps BLKmode and inherits nothing.
// CHECK-LABEL: define dso_local void @named_sld2([4 x i64]
void named_sld2(struct sld2 x) { (void)x; }

// A union never inherits, even with one member that fills it.
// CHECK-LABEL: define dso_local void @named_uld([2 x i64]
void named_uld(union uld x) { (void)x; }

// The member has to fill the struct; the padded one is four quadwords, not
// two, because the int sits in a 16-byte-aligned tail.
// CHECK-LABEL: define dso_local void @named_sldp([4 x i64]
void named_sldp(struct sldp x) { (void)x; }

// A single float or _Complex float member is by value when it is named: only
// an *unnamed* SFmode/SCmode argument goes by reference.
// CHECK-LABEL: define dso_local void @named_sf(i64
void named_sf(struct sf x) { (void)x; }

// CHECK-LABEL: define dso_local void @named_sscf(i64
void named_sscf(struct sscf x) { (void)x; }

// In the variadic tail the same structs go by reference, one address in one
// slot.  A bare _Complex float is two addresses in two slots, because gcc
// splits the complex *argument* before classifying either half; a struct that
// merely has complex mode is not split.
void sink(long, ...);

// CHECK-LABEL: define dso_local void @pass_sf(
// CHECK: call void (i64, ...) @sink(i64 noundef 1, ptr noundef byval(%struct.sf)
void pass_sf(struct sf x) { sink(1, x, 9L); }

// CHECK-LABEL: define dso_local void @pass_sf1(
// CHECK: call void (i64, ...) @sink(i64 noundef 1, ptr noundef byval(%struct.sf1)
void pass_sf1(struct sf1 x) { sink(1, x, 9L); }

// CHECK-LABEL: define dso_local void @pass_sfn(
// CHECK: call void (i64, ...) @sink(i64 noundef 1, ptr noundef byval(%struct.sfn)
void pass_sfn(struct sfn x) { sink(1, x, 9L); }

// CHECK-LABEL: define dso_local void @pass_sscf(
// CHECK: call void (i64, ...) @sink(i64 noundef 1, ptr noundef byval(%struct.sscf)
void pass_sscf(struct sscf x) { sink(1, x, 9L); }

// CHECK-LABEL: define dso_local void @pass_sscfn(
// CHECK: call void (i64, ...) @sink(i64 noundef 1, ptr noundef byval(%struct.sscfn)
void pass_sscfn(struct sscfn x) { sink(1, x, 9L); }

// CHECK-LABEL: define dso_local void @pass_sld(
// CHECK: call void (i64, ...) @sink(i64 noundef 1, ptr noundef byval(%struct.sld)
void pass_sld(struct sld x) { sink(1, x, 9L); }

// The boundaries, in the same position: two members, a union, a double member
// and a _Complex double member all stay by value.  Without these the rule
// could widen to every single-member struct and nothing here would notice.
// CHECK-LABEL: define dso_local void @pass_sf2(
// CHECK: call void (i64, ...) @sink(i64 noundef 1, i64
void pass_sf2(struct sf2 x) { sink(1, x, 9L); }

// CHECK-LABEL: define dso_local void @pass_uf(
// CHECK: call void (i64, ...) @sink(i64 noundef 1, i64
void pass_uf(union uf x) { sink(1, x, 9L); }

// CHECK-LABEL: define dso_local void @pass_uf1(
// CHECK: call void (i64, ...) @sink(i64 noundef 1, i64
void pass_uf1(union uf1 x) { sink(1, x, 9L); }

// CHECK-LABEL: define dso_local void @pass_uscf(
// CHECK: call void (i64, ...) @sink(i64 noundef 1, i64
void pass_uscf(union uscf x) { sink(1, x, 9L); }

// CHECK-LABEL: define dso_local void @pass_sd(
// CHECK: call void (i64, ...) @sink(i64 noundef 1, i64
void pass_sd(struct sd x) { sink(1, x, 9L); }

// CHECK-LABEL: define dso_local void @pass_sscd(
// CHECK: call void (i64, ...) @sink(i64 noundef 1, [2 x i64]
void pass_sscd(struct sscd x) { sink(1, x, 9L); }

// va_arg reads them back the same way, because it classifies with IsNamed
// false and dereferences an indirect argument.
typedef __builtin_va_list va_list;

// CHECK-LABEL: define dso_local float @get_sf(
// CHECK: load ptr, ptr %ap.cur
float get_sf(va_list ap) { return __builtin_va_arg(ap, struct sf).f; }

// A member of zero size is walked over, not counted, so these still inherit:
// a zero-length array, a GNU empty struct, a zero-width bitfield.
struct fz  { float f; int a[0]; };
struct lz  { long double x; int a[0]; };
struct fe  { float f; struct {} e; };
struct zf  { int : 0; float f; };
struct lzb { long double x; int : 0; };

// CHECK-LABEL: define dso_local void @named_lz(
// CHECK-SAME: ptr noundef byval(%struct.lz)
void named_lz(struct lz x) { (void)x; }

// CHECK-LABEL: define dso_local void @named_lzb(
// CHECK-SAME: ptr noundef byval(%struct.lzb)
void named_lzb(struct lzb x) { (void)x; }

// CHECK-LABEL: define dso_local void @pass_fz(
// CHECK: call void (i64, ...) @sink(i64 noundef 1, ptr noundef byval(%struct.fz)
void pass_fz(struct fz x) { sink(1, x, 9L); }

// CHECK-LABEL: define dso_local void @pass_fe(
// CHECK: call void (i64, ...) @sink(i64 noundef 1, ptr noundef byval(%struct.fe)
void pass_fe(struct fe x) { sink(1, x, 9L); }

// CHECK-LABEL: define dso_local void @pass_zf(
// CHECK: call void (i64, ...) @sink(i64 noundef 1, ptr noundef byval(%struct.zf)
void pass_zf(struct zf x) { sink(1, x, 9L); }

// A non-zero-width bitfield is a member like any other and stops the walk.
struct bff { float f; int b : 1; };

// CHECK-LABEL: define dso_local void @pass_bff(
// CHECK: call void (i64, ...) @sink(i64 noundef 1, i64
void pass_bff(struct bff x) { sink(1, x, 9L); }

// packed drops the record's alignment below the mode's, so it keeps BLKmode
// and is passed as quadwords -- not by reference.
struct pl { long double x; } __attribute__((packed));
struct pf { float f; } __attribute__((packed));

// CHECK-LABEL: define dso_local void @named_pl([2 x i64]
void named_pl(struct pl x) { (void)x; }

// CHECK-LABEL: define dso_local void @pass_pf(
// CHECK: call void (i64, ...) @sink(i64 noundef 1, i64
void pass_pf(struct pf x) { sink(1, x, 9L); }
