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
#include "llvm/MC/TargetRegistry.h"
#include "llvm/Support/Endian.h"

using namespace llvm;
using namespace llvm::MCD;

#define DEBUG_TYPE "alpha-disassembler"

typedef MCDisassembler::DecodeStatus DecodeStatus;

namespace {
class AlphaDisassembler : public MCDisassembler {
public:
  AlphaDisassembler(const MCSubtargetInfo &STI, MCContext &Ctx)
      : MCDisassembler(STI, Ctx) {}
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

// A 21-bit signed PC-relative branch displacement in instruction units.
static DecodeStatus decodeBranchTarget(MCInst &Inst, uint64_t Imm,
                                       uint64_t Address,
                                       const MCDisassembler *Decoder) {
  Inst.addOperand(MCOperand::createImm(SignExtend64<21>(Imm)));
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
  return decodeInstruction(DecoderTable32, Instr, Insn, Address, this, STI);
}

static MCDisassembler *createAlphaDisassembler(const Target &T,
                                               const MCSubtargetInfo &STI,
                                               MCContext &Ctx) {
  return new AlphaDisassembler(STI, Ctx);
}

extern "C" LLVM_EXTERNAL_VISIBILITY void LLVMInitializeAlphaDisassembler() {
  TargetRegistry::RegisterMCDisassembler(getTheAlphaTarget(),
                                         createAlphaDisassembler);
}
