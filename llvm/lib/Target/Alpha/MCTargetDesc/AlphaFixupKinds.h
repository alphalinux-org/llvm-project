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
  // initial-exec TLS (R_ALPHA_GOTTPREL).
  fixup_alpha_gottprel,
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

  fixup_alpha_invalid,
  NumTargetFixupKinds = fixup_alpha_invalid - FirstTargetFixupKind
};

// The `!name` relocation-specifier suffix that selects the given fixup kind
// (empty for kinds without a specifier spelling).
inline StringRef getSpecifierName(unsigned Kind) {
  switch (Kind) {
  case fixup_alpha_literal:
    return "literal";
  case fixup_alpha_gprelhigh:
    return "gprelhigh";
  case fixup_alpha_gprellow:
    return "gprellow";
  case fixup_alpha_gprel16:
    return "gprel";
  case fixup_alpha_gpdisp:
    return "gpdisp";
  case fixup_alpha_tprelhi:
    return "tprelhi";
  case fixup_alpha_tprello:
    return "tprello";
  case fixup_alpha_gottprel:
    return "gottprel";
  case fixup_alpha_tlsgd:
    return "tlsgd";
  case fixup_alpha_tlsldm:
    return "tlsldm";
  case fixup_alpha_dtprelhi:
    return "dtprelhi";
  case fixup_alpha_dtprello:
    return "dtprello";
  case fixup_alpha_brsgp:
    return "samegp";
  default:
    return "";
  }
}
} // namespace Alpha
} // namespace llvm

#endif
