//===-- AlphaDisassembler.cpp - Disassembler for Alpha -------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "MCTargetDesc/AlphaMCTargetDesc.h"
#include "TargetInfo/AlphaTargetInfo.h"
#include "llvm/MC/MCContext.h"
#include "llvm/MC/MCDecoder.h"
#include "llvm/MC/MCDecoderOps.h"
#include "llvm/MC/MCDisassembler/MCDisassembler.h"
#include "llvm/MC/MCInst.h"
#include "llvm/MC/MCInstrInfo.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/Support/Endian.h"

using namespace llvm;
using namespace llvm::MCD;

#define DEBUG_TYPE "alpha-disassembler"

typedef MCDisassembler::DecodeStatus DecodeStatus;

// The trap and rounding qualifier a floating-point operate word carries, as
// MCInst flags.  Rounding is bits 7:6 of the function field, where the
// unqualified form rounds to nearest.
static unsigned qualFromWord(uint32_t Insn) {
  unsigned Func = (Insn >> 5) & 0x7ff;
  unsigned RoundBits = Func & 0x0c0;
  unsigned RM = RoundBits == 0x000   ? Alpha::FPRoundChopped
                : RoundBits == 0x040 ? Alpha::FPRoundMinus
                : RoundBits == 0x0c0 ? Alpha::FPRoundDynamic
                                     : Alpha::FPRoundNormal;
  return Alpha::encodeFPQual(Func & 0x700, RM);
}

namespace {
class AlphaDisassembler : public MCDisassembler {
  std::unique_ptr<const MCInstrInfo> MCII;

public:
  AlphaDisassembler(const MCSubtargetInfo &STI, MCContext &Ctx,
                    std::unique_ptr<const MCInstrInfo> MCII)
      : MCDisassembler(STI, Ctx), MCII(std::move(MCII)) {}
  ~AlphaDisassembler() override = default;

  DecodeStatus getInstruction(MCInst &Instr, uint64_t &Size,
                              ArrayRef<uint8_t> Bytes, uint64_t Address,
                              raw_ostream &CStream) const override;
};
} // end anonymous namespace

static const MCPhysReg GPRDecoderTable[] = {
    Alpha::R0,  Alpha::R1,  Alpha::R2,  Alpha::R3,  Alpha::R4,  Alpha::R5,
    Alpha::R6,  Alpha::R7,  Alpha::R8,  Alpha::R9,  Alpha::R10, Alpha::R11,
    Alpha::R12, Alpha::R13, Alpha::R14, Alpha::R15, Alpha::R16, Alpha::R17,
    Alpha::R18, Alpha::R19, Alpha::R20, Alpha::R21, Alpha::R22, Alpha::R23,
    Alpha::R24, Alpha::R25, Alpha::R26, Alpha::R27, Alpha::R28, Alpha::R29,
    Alpha::R30, Alpha::R31};

static const MCPhysReg FPRDecoderTable[] = {
    Alpha::F0,  Alpha::F1,  Alpha::F2,  Alpha::F3,  Alpha::F4,  Alpha::F5,
    Alpha::F6,  Alpha::F7,  Alpha::F8,  Alpha::F9,  Alpha::F10, Alpha::F11,
    Alpha::F12, Alpha::F13, Alpha::F14, Alpha::F15, Alpha::F16, Alpha::F17,
    Alpha::F18, Alpha::F19, Alpha::F20, Alpha::F21, Alpha::F22, Alpha::F23,
    Alpha::F24, Alpha::F25, Alpha::F26, Alpha::F27, Alpha::F28, Alpha::F29,
    Alpha::F30, Alpha::F31};

static DecodeStatus DecodeGPRCRegisterClass(MCInst &Inst, uint64_t RegNo,
                                            uint64_t Address,
                                            const MCDisassembler *Decoder) {
  if (RegNo > 31)
    return MCDisassembler::Fail;
  Inst.addOperand(MCOperand::createReg(GPRDecoderTable[RegNo]));
  return MCDisassembler::Success;
}

static DecodeStatus DecodeFPRCRegisterClass(MCInst &Inst, uint64_t RegNo,
                                            uint64_t Address,
                                            const MCDisassembler *Decoder) {
  if (RegNo > 31)
    return MCDisassembler::Fail;
  Inst.addOperand(MCOperand::createReg(FPRDecoderTable[RegNo]));
  return MCDisassembler::Success;
}

// memri: base register in bits 20-16, 16-bit signed displacement in bits 15-0.
static DecodeStatus decodeMemri(MCInst &Inst, uint64_t Imm, uint64_t Address,
                                const MCDisassembler *Decoder) {
  unsigned Base = (Imm >> 16) & 0x1f;
  int64_t Disp = SignExtend64<16>(Imm & 0xffff);
  Inst.addOperand(MCOperand::createReg(GPRDecoderTable[Base]));
  Inst.addOperand(MCOperand::createImm(Disp));
  return MCDisassembler::Success;
}

// A 16-bit displacement filled by a relocation; decode the immediate as-is.
static DecodeStatus decodeImm(MCInst &Inst, uint64_t Imm, uint64_t Address,
                              const MCDisassembler *Decoder) {
  Inst.addOperand(MCOperand::createImm(SignExtend64<16>(Imm & 0xffff)));
  return MCDisassembler::Success;
}

// A 21-bit signed PC-relative branch displacement in instruction units.  Give
// the target's address to the symbolizer so a disassembly names the callee the
// way every other target's does, rather than printing the raw displacement and
// leaving the reader to do the arithmetic.
static DecodeStatus decodeBranchTarget(MCInst &Inst, uint64_t Imm,
                                       uint64_t Address,
                                       const MCDisassembler *Decoder) {
  int64_t Disp = SignExtend64<21>(Imm);
  // The displacement counts instructions from the one after the branch.
  uint64_t Target = Address + 4 + Disp * 4;
  if (!Decoder->tryAddingSymbolicOperand(Inst, Disp, Address, /*IsBranch=*/true,
                                         /*Offset=*/0, /*OpSize=*/0,
                                         /*InstSize=*/4))
    Inst.addOperand(MCOperand::createImm(Disp));
  (void)Target;
  return MCDisassembler::Success;
}

// mf_fpcr/mt_fpcr encode their one floating register in all three register
// fields; read it from Rc (bits 4-0).  A dedicated method avoids the ambiguity
// of the same operand appearing three times.
static DecodeStatus decodeFpcrMove(MCInst &Inst, uint32_t Insn,
                                   uint64_t Address,
                                   const MCDisassembler *Decoder) {
  // All three fields name the same register, and a word where they disagree is
  // not this instruction -- GNU as will not assemble one and GNU objdump does
  // not decode one.
  unsigned Ra = (Insn >> 21) & 0x1f, Rb = (Insn >> 16) & 0x1f, Rc = Insn & 0x1f;
  if (Ra != Rc || Rb != Rc)
    return MCDisassembler::Fail;
  Inst.addOperand(MCOperand::createReg(FPRDecoderTable[Rc]));
  return MCDisassembler::Success;
}

#include "AlphaGenDisassemblerTables.inc"

DecodeStatus AlphaDisassembler::getInstruction(MCInst &Instr, uint64_t &Size,
                                               ArrayRef<uint8_t> Bytes,
                                               uint64_t Address,
                                               raw_ostream &CStream) const {
  if (Bytes.size() < 4) {
    Size = 0;
    return MCDisassembler::Fail;
  }
  Size = 4;
  uint32_t Insn = support::endian::read32le(Bytes.data());
  DecodeStatus S =
      decodeInstruction(DecoderTable32, Instr, Insn, Address, this, STI);
  if (S != MCDisassembler::Fail) {
    // Record what the bits say about the qualifier, so the printer shows that
    // rather than deriving one from -mattr: the same word must not read as
    // `addt' or `addt/su' depending on a flag.
    if (unsigned TrapClass = MCII->get(Instr.getOpcode()).TSFlags & Alpha::TrapClassMask) {
      unsigned Qual = qualFromWord(Insn);
      unsigned TrapBits = Alpha::fpTrapFieldIsQualifier(TrapClass)
                              ? Alpha::fpQualTrapBits(Qual)
                              : 0;
      // A class that does not take the ambient rounding mode has a table entry
      // per rounding field -- cvttq/c and cvttq are separate defs -- so its
      // mnemonic already spells the mode.  Recording it again would print it
      // twice, as cvttq/cc.
      unsigned RM = Alpha::fpRounds(TrapClass) ? Alpha::fpQualRoundMode(Qual)
                                               : Alpha::FPRoundNormal;
      if (!Alpha::fpQualIsLegal(TrapClass, TrapBits, RM))
        return MCDisassembler::Fail;
      Instr.setFlags(Alpha::encodeFPQual(TrapBits, RM));
    }
    return S;
  }

  // A floating-point operate carries its trap and rounding qualifiers in the
  // function field, and only the unqualified encodings have a table entry.  So
  // an instruction assembled with -mieee -- including one this compiler
  // produced -- would not decode at all.  Take the qualifier out, decode what
  // is left, and hand the qualifier to the printer through the instruction's
  // flags so it prints what the bits actually say.
  // 0x14 is the FIX/CIX operate group, which sqrts and sqrtt live in and which
  // carries the same qualifier field; leaving it out left every qualified
  // square root in the platform's own libm undecodable.
  unsigned Opcode = Insn >> 26;
  if (Opcode != 0x14 && Opcode != 0x15 && Opcode != 0x16 && Opcode != 0x17)
    return MCDisassembler::Fail;

  unsigned Func = (Insn >> 5) & 0x7ff;
  unsigned TrapBits = Func & 0x700;
  unsigned RM = Alpha::fpQualRoundMode(qualFromWord(Insn));
  if (!TrapBits && RM == Alpha::FPRoundNormal)
    return MCDisassembler::Fail;

  // Two ways the qualifier can sit on top of a base encoding.  Most operates
  // have the round-to-nearest bits in their own function field, so taking the
  // rounding letter out means restoring those; but cvtql has no rounding field
  // at all and its base encoding leaves those bits clear, so restoring them
  // would name a different instruction.  Try the common shape first and the
  // other only if nothing decodes.
  const struct {
    uint32_t Base;
    unsigned RM;
  } Candidates[] = {
      // The rounding letter comes out of the function field, so the base
      // encoding's own round-to-nearest bits go back in.
      {(Insn & ~(0x7ffu << 5)) | ((Func & ~0x7c0u) << 5) | (0x080u << 5), RM},
  };
  MCInst Bare;
  unsigned TrapClass = 0;
  S = MCDisassembler::Fail;
  for (const auto &C : Candidates) {
    MCInst Try;
    if (decodeInstruction(DecoderTable32, Try, C.Base, Address, this, STI) ==
        MCDisassembler::Fail)
      continue;
    unsigned TC = MCII->get(Try.getOpcode()).TSFlags & Alpha::TrapClassMask;
    if (!TC || !Alpha::fpQualIsLegal(TC, TrapBits, C.RM))
      continue;
    Bare = Try;
    TrapClass = TC;
    RM = C.RM;
    S = MCDisassembler::Success;
    break;
  }
  if (S == MCDisassembler::Fail)
    return S;
  Instr = Bare;
  Instr.setFlags(Alpha::encodeFPQual(TrapBits, RM));
  return S;
}

static MCDisassembler *createAlphaDisassembler(const Target &T,
                                               const MCSubtargetInfo &STI,
                                               MCContext &Ctx) {
  return new AlphaDisassembler(
      STI, Ctx, std::unique_ptr<const MCInstrInfo>(T.createMCInstrInfo()));
}

extern "C" LLVM_EXTERNAL_VISIBILITY void LLVMInitializeAlphaDisassembler() {
  TargetRegistry::RegisterMCDisassembler(getTheAlphaTarget(),
                                         createAlphaDisassembler);
}
