//===- MCSymbolELFTest.cpp ------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "llvm/MC/MCSymbolELF.h"
#include "llvm/BinaryFormat/ELF.h"
#include "llvm/MC/MCAsmInfo.h"
#include "llvm/MC/MCContext.h"
#include "llvm/MC/MCRegisterInfo.h"
#include "llvm/MC/MCSubtargetInfo.h"
#include "llvm/MC/MCSymbol.h"
#include "llvm/MC/MCTargetOptions.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/Support/TargetSelect.h"
#include "gtest/gtest.h"

using namespace llvm;

namespace {

struct Context {
  static constexpr char TripleName[] = "x86_64-pc-linux";
  Triple TT;
  std::unique_ptr<MCRegisterInfo> MRI;
  std::unique_ptr<MCAsmInfo> MAI;
  std::unique_ptr<MCSubtargetInfo> STI;
  std::unique_ptr<MCContext> Ctx;

  Context() : TT(TripleName) {
    llvm::InitializeAllTargetInfos();
    llvm::InitializeAllTargetMCs();

    // If we didn't build x86, do not run the test.
    std::string Error;
    const Target *TheTarget = TargetRegistry::lookupTarget(TT, Error);
    if (!TheTarget)
      return;

    MRI.reset(TheTarget->createMCRegInfo(TT));
    MCTargetOptions MCOptions;
    MAI.reset(TheTarget->createMCAsmInfo(*MRI, TT, MCOptions));
    STI.reset(TheTarget->createMCSubtargetInfo(TT, "", ""));
    Ctx = std::make_unique<MCContext>(TT, *MAI, *MRI, *STI);
  }

  operator bool() { return Ctx.get(); }
  operator MCContext &() { return *Ctx; }
};

Context &getContext() {
  static Context Ctxt;
  return Ctxt;
}

MCSymbolELF *sym(const char *Name) {
  MCContext &Ctx = getContext();
  return static_cast<MCSymbolELF *>(Ctx.getOrCreateSymbol(Name));
}

// st_other holds five processor-specific bits, 3 through 7.  They are stored
// shifted down by three, so a value whose bit 3 is set has to survive the round
// trip: Alpha's STO_ALPHA_STD_GPLOAD is 0x88, and a narrower field silently
// dropped its 0x08.
TEST(MCSymbolELFTest, OtherRoundTripsAllFiveBits) {
  if (!getContext())
    GTEST_SKIP() << "x86 target not built";
  MCSymbolELF *S = sym("round_trip");
  for (unsigned Value = 0; Value < 0x100; Value += 8) {
    S->setOther(Value);
    EXPECT_EQ(Value, S->getOther()) << "st_other " << Value;
  }
}

TEST(MCSymbolELFTest, OtherKeepsAlphaProcedureKinds) {
  if (!getContext())
    GTEST_SKIP() << "x86 target not built";
  MCSymbolELF *S = sym("procedure_kind");
  S->setOther(0x88); // STO_ALPHA_STD_GPLOAD
  EXPECT_EQ(0x88u, S->getOther());
  S->setOther(0x80); // STO_ALPHA_NOPV
  EXPECT_EQ(0x80u, S->getOther());
}

// Visibility and binding live in the same byte but in their own fields, so
// setting one must not disturb the others.
TEST(MCSymbolELFTest, OtherVisibilityAndBindingAreIndependent) {
  if (!getContext())
    GTEST_SKIP() << "x86 target not built";
  MCSymbolELF *S = sym("independent");
  S->setOther(0x88);
  S->setVisibility(ELF::STV_HIDDEN);
  EXPECT_EQ(0x88u, S->getOther());
  EXPECT_EQ(unsigned(ELF::STV_HIDDEN), S->getVisibility());

  S->setBinding(ELF::STB_WEAK);
  EXPECT_EQ(0x88u, S->getOther());
  EXPECT_EQ(unsigned(ELF::STV_HIDDEN), S->getVisibility());
  EXPECT_EQ(unsigned(ELF::STB_WEAK), S->getBinding());
}

} // namespace
