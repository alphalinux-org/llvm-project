//===-- AlphaMCAsmInfo.cpp - Alpha Asm Properties -------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "AlphaMCAsmInfo.h"
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
  // The .Nbyte spellings are the ones GNU as does not align to their own
  // width (s_alpha_ucons, "Dwarf wants these versions of unaligned").  Every
  // object this prints is preceded by an explicit .p2align, so nothing here
  // wants the implicit one -- and the DWARF sections, which pack items of
  // mixed width, are corrupted by it.  gcc spells its DWARF the same way.
  Data16bitsDirective = "\t.2byte\t";
  Data32bitsDirective = "\t.4byte\t";
  Data64bitsDirective = "\t.8byte\t";
  GlobalDirective = "\t.globl\t";
  UsesELFSectionDirectiveForBSS = true;
  SupportsDebugInformation = true;
  ExceptionsType = ExceptionHandling::DwarfCFI;
}
