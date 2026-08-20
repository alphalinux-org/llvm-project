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

  fixup_alpha_invalid,
  NumTargetFixupKinds = fixup_alpha_invalid - FirstTargetFixupKind
};
} // namespace Alpha
} // namespace llvm

#endif
