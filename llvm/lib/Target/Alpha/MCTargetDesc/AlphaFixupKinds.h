//===-- AlphaFixupKinds.h - Alpha-specific fixup entries --------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_ALPHA_MCTARGETDESC_ALPHAFIXUPKINDS_H
#define LLVM_LIB_TARGET_ALPHA_MCTARGETDESC_ALPHAFIXUPKINDS_H

#include "llvm/ADT/StringRef.h"
#include "llvm/MC/MCFixup.h"

namespace llvm {
namespace Alpha {
enum Fixups {
  // A 21-bit PC-relative branch displacement (R_ALPHA_BRADDR).
  fixup_alpha_braddr = FirstTargetFixupKind,
  // A 16-bit GOT-relative literal displacement (R_ALPHA_LITERAL).
  fixup_alpha_literal,
  // The high/low 16-bit GP-relative displacements (R_ALPHA_GPRELHIGH/LOW).
  fixup_alpha_gprelhigh,
  fixup_alpha_gprellow,
  // A 16-bit GP-relative displacement in a load/store offset (R_ALPHA_GPREL16),
  // from a `!gprel` suffix.
  fixup_alpha_gprel16,
  // The ldah of an ldgp pair (R_ALPHA_GPDISP); its addend is the byte distance
  // to the matching lda.
  fixup_alpha_gpdisp,
  // The high/low 16-bit thread-pointer-relative displacements for local-exec
  // TLS (R_ALPHA_TPRELHI/LO).
  fixup_alpha_tprelhi,
  fixup_alpha_tprello,
  // A 16-bit GOT displacement to a thread-pointer-relative offset for
  // initial-exec TLS (R_ALPHA_GOTTPREL), and to a module-relative offset for
  // local-dynamic TLS (R_ALPHA_GOTDTPREL).
  fixup_alpha_gottprel,
  fixup_alpha_gotdtprel,
  // A 16-bit GOT displacement to the general-dynamic TLS descriptor passed to
  // __tls_get_addr (R_ALPHA_TLSGD).
  fixup_alpha_tlsgd,
  // The local-dynamic module descriptor (R_ALPHA_TLSLDM) and the high/low
  // module-relative offsets that follow it (R_ALPHA_DTPRELHI/LO).
  fixup_alpha_tlsldm,
  fixup_alpha_dtprelhi,
  fixup_alpha_dtprello,
  // A jsr branch-prediction hint filled from the call target (R_ALPHA_HINT),
  // and the paired use of a GOT literal by a jsr (R_ALPHA_LITUSE, addend 3)
  // that lets the linker relax a local call.
  fixup_alpha_hint,
  fixup_alpha_lituse_jsr,
  // A 21-bit PC-relative branch to a routine sharing the caller's global
  // pointer (R_ALPHA_BRSGP), from a `!samegp` suffix.
  fixup_alpha_brsgp,
  // A 32-bit GP-relative value (R_ALPHA_GPREL32), from a `.gprel32` directive.
  fixup_alpha_gprel32,
  // An expression with no relocation specifier in the 16-bit displacement of a
  // memory-format instruction, or in the 8-bit literal of an operate-format
  // one.  These carry no relocation: the assembler has to fold the expression
  // to a constant -- a difference of two labels in the same section, say -- and
  // it is an error if it cannot.
  fixup_alpha_disp16,
  fixup_alpha_lit8,
  // Not a relocation anyone asked for: a placeholder emitted next to a
  // `!literal!N' and to the `!lituse_*!N' that names it, carrying N so that
  // AlphaELFObjectWriter::sortRelocs can put the pair next to each other in
  // .rela.text the way GNU as does.  It writes nothing into the instruction
  // and is removed from the relocation table before the object is written, so
  // it never reaches a file.
  fixup_alpha_seqmark,

  fixup_alpha_invalid,
  NumTargetFixupKinds = fixup_alpha_invalid - FirstTargetFixupKind
};

// Every relocation specifier, in the one place both directions read: the
// assembler turns a `!name' suffix into a fixup kind, the printer turns a kind
// back into the suffix it writes.  A row added to only one of two lists would
// make the assembler accept a suffix the printer cannot write, or the other way
// round -- which is exactly what an `llvm-mc | llvm-mc' round trip does.
//
// fixup_alpha_gprel32 is deliberately absent.  GNU as attaches a `!'-suffix to
// an instruction operand, never to a data directive, so there is neither a
// `!gprel32' to print nor one to read; the only spelling is the .gprel32
// directive, which AlphaAsmPrinter::emitJumpTableEntry writes.
struct SpecifierInfo {
  unsigned Kind;
  StringRef Name;
};

inline constexpr SpecifierInfo SpecifierInfos[] = {
    {fixup_alpha_literal, "literal"},     {fixup_alpha_gprelhigh, "gprelhigh"},
    {fixup_alpha_gprellow, "gprellow"},   {fixup_alpha_gprel16, "gprel"},
    {fixup_alpha_gpdisp, "gpdisp"},       {fixup_alpha_tprelhi, "tprelhi"},
    {fixup_alpha_tprello, "tprello"},     {fixup_alpha_gottprel, "gottprel"},
    {fixup_alpha_gotdtprel, "gotdtprel"}, {fixup_alpha_tlsgd, "tlsgd"},
    {fixup_alpha_tlsldm, "tlsldm"},       {fixup_alpha_dtprelhi, "dtprelhi"},
    {fixup_alpha_dtprello, "dtprello"},   {fixup_alpha_brsgp, "samegp"},
};

// The `!name` relocation-specifier suffix that selects the given fixup kind
// (empty for kinds without a specifier spelling).
inline StringRef getSpecifierName(unsigned Kind) {
  for (const SpecifierInfo &S : SpecifierInfos)
    if (S.Kind == Kind)
      return S.Name;
  return "";
}

// The fixup kind a `!name` suffix selects, or zero for a name that is not a
// relocation specifier.  Zero is not a fixup kind: the target kinds start at
// MCFixupKind::FirstTargetFixupKind.
inline unsigned getSpecifierKind(StringRef Name) {
  for (const SpecifierInfo &S : SpecifierInfos)
    if (S.Name == Name)
      return S.Kind;
  return 0;
}
} // namespace Alpha
} // namespace llvm

#endif
