//===-- AlphaMCAsmInfo.cpp - Alpha Asm Properties -------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "AlphaMCAsmInfo.h"
#include "AlphaFixupKinds.h"
#include "llvm/MC/MCExpr.h"
#include "llvm/MC/MCTargetOptions.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/TargetParser/Triple.h"

using namespace llvm;

AlphaMCAsmInfo::AlphaMCAsmInfo(const Triple &TT, const MCTargetOptions &Options)
    : MCAsmInfoELF(Options) {
  CodePointerSize = 8;
  CalleeSaveStackSlotSize = 8;
  IsLittleEndian = true;
  CommentString = "#";
  // `.align N` aligns to a 2^N boundary, matching GNU as on Alpha.
  AlignmentIsInBytes = false;
  // A trailing `!name` is a relocation specifier, not an infix `!` operator.
  UseExclaimForSpecifier = true;
  Data64bitsDirective = "\t.quad\t";
  GlobalDirective = "\t.globl\t";
  UsesELFSectionDirectiveForBSS = true;
  // An alias inherits the procedure kind of what it names.  These are the only
  // st_other bits alpha defines, and a caller reaching a function through an
  // alias needs to know whether it can skip the gp-load prologue just as much
  // as one reaching it by name.  GNU as propagates them the same way.
  // STO_ALPHA_NOPV | STO_ALPHA_STD_GPLOAD; see AlphaAsmPrinter, which sets
  // them, and lld/ELF/Arch/Alpha.cpp, which reads them.
  InheritedSTOtherMask = 0x88;
  SupportsDebugInformation = true;
  ExceptionsType = ExceptionHandling::DwarfCFI;
}

void AlphaMCAsmInfo::printSpecifierExpr(raw_ostream &OS,
                                        const MCSpecifierExpr &Expr) const {
  // A relocation specifier prints as the subexpression followed by `!name`
  // (the specifier value is the Alpha fixup kind it selects).
  printExpr(OS, *Expr.getSubExpr());
  StringRef Name = Alpha::getSpecifierName(Expr.getSpecifier());
  if (!Name.empty())
    OS << " !" << Name;
}
