//===--- alpha.h - Generic JITLink alpha edge kinds, utilities --*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Generic utilities for graphs representing alpha objects.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_EXECUTIONENGINE_JITLINK_ALPHA_H
#define LLVM_EXECUTIONENGINE_JITLINK_ALPHA_H

#include "llvm/ExecutionEngine/JITLink/JITLink.h"
#include "llvm/ExecutionEngine/JITLink/TableManager.h"

namespace llvm::jitlink::alpha {

/// Represents alpha fixups and other alpha-specific edge kinds.
enum EdgeKind_alpha : Edge::Kind {

  /// A plain 64-bit pointer value relocation.
  ///
  /// Fixup expression:
  ///   Fixup <- Target + Addend : uint64
  ///
  Pointer64 = Edge::FirstRelocation,

  /// A plain 32-bit pointer value relocation.
  ///
  /// Fixup expression:
  ///   Fixup <- Target + Addend : uint32
  ///
  /// Errors:
  ///   - The target must reside in the low 32-bits of the address space.
  ///
  Pointer32,

  /// A 64-bit delta.
  ///
  /// Fixup expression:
  ///   Fixup <- Target - Fixup + Addend : int64
  ///
  Delta64,

  /// A 32-bit delta.
  ///
  /// Fixup expression:
  ///   Fixup <- Target - Fixup + Addend : int32
  ///
  Delta32,

  /// A 32-bit negative delta, used by eh-frame parsing.
  ///
  /// Fixup expression:
  ///   Fixup <- Fixup - Target + Addend : int32
  ///
  NegDelta32,

  /// A 21-bit PC-relative branch.
  ///
  /// A branch displacement counts instructions from the one after the branch.
  ///
  /// Fixup expression:
  ///   Fixup <- (Target - (Fixup + 4) + Addend) >> 2 : int21
  ///
  /// Errors:
  ///   - The result must be an in-range int21.
  ///   - The target must be 4-byte aligned.
  ///
  Branch21PCRel,

  /// The branch-prediction hint of a jsr, whose value only affects performance.
  ///
  /// Fixup expression:
  ///   Fixup <- (Target - (Fixup + 4) + Addend) >> 2 : int14 (truncated)
  ///
  Hint14PCRel,

  /// The high half of a displacement from the global pointer, with the carry
  /// out of the low half folded in.
  ///
  /// Fixup expression:
  ///   Fixup <- (Target - GP + Addend + 0x8000) >> 16 : int16
  ///
  GPRelHigh16,

  /// The low half of a displacement from the global pointer.
  ///
  /// Fixup expression:
  ///   Fixup <- (Target - GP + Addend) & 0xffff : int16
  ///
  GPRelLow16,

  /// A 16-bit displacement from the global pointer.
  ///
  /// Fixup expression:
  ///   Fixup <- Target - GP + Addend : int16
  ///
  /// Errors:
  ///   - The result must be an in-range int16.
  ///
  GPRel16,

  /// A 32-bit displacement from the global pointer.
  ///
  /// Fixup expression:
  ///   Fixup <- Target - GP + Addend : int32
  ///
  /// Errors:
  ///   - The result must be an in-range int32.
  ///
  GPRel32,

  /// The ldah/lda pair that forms the global pointer at a function's entry.
  ///
  /// The fixup sits on the ldah; the addend is the distance from it to the lda,
  /// which is 4 unless something was scheduled between the two.  Both
  /// instructions are patched: the high half of the displacement (with the
  /// carry out of the low half folded in) into the ldah, the low half into the
  /// lda.  The edge's target is ignored -- the value is fixed by where the
  /// global pointer and the ldah are.
  ///
  /// Fixup expression:
  ///   ldah <- (GP - Fixup + 0x8000) >> 16 : int16
  ///   lda  <- (GP - Fixup) & 0xffff : int16
  ///
  GPDisp,

  /// A GOT entry for the target, addressed as a displacement from the global
  /// pointer.
  ///
  /// Fixup expression:
  ///   Fixup <- GOTEntry - GP + Addend : int16
  ///
  /// Errors:
  ///   - The result must be an in-range int16, which caps the reach of a single
  ///     global pointer at 64k of GOT entries.
  ///
  RequestGOTAndTransformToGPRel16,

  /// A call to the PLT entry for the target, as a 21-bit PC-relative branch.
  ///
  BranchPCRel21ToPLT,
};

/// Returns a string name for the given alpha edge. For debugging purposes only.
const char *getEdgeKindName(Edge::Kind K);

/// The global pointer covers a 64k window centered on the GOT, so it points
/// 0x8000 bytes past its start.  Every displacement from it is a signed 16-bit
/// value, which is where the reach of a single global pointer comes from.
inline orc::ExecutorAddr getGPForGOT(const Symbol *GOTSymbol) {
  assert(GOTSymbol && "No GOT symbol");
  return GOTSymbol->getAddress() + 0x8000;
}

/// Apply fixup expression for edge to block content.
inline Error applyFixup(LinkGraph &G, Block &B, const Edge &E,
                        const Symbol *GOTSymbol) {
  using namespace support;

  char *BlockWorkingMem = B.getAlreadyMutableContent().data();
  char *FixupPtr = BlockWorkingMem + E.getOffset();
  auto FixupAddress = B.getAddress() + E.getOffset();

  switch (E.getKind()) {
  case Pointer64: {
    uint64_t Value = E.getTarget().getAddress().getValue() + E.getAddend();
    endian::write64le(FixupPtr, Value);
    break;
  }
  case Pointer32: {
    uint64_t Value = E.getTarget().getAddress().getValue() + E.getAddend();
    if (!isUInt<32>(Value) && !isInt<32>(static_cast<int64_t>(Value)))
      return makeTargetOutOfRangeError(G, B, E);
    endian::write32le(FixupPtr, static_cast<uint32_t>(Value));
    break;
  }
  case Delta64: {
    int64_t Value = E.getTarget().getAddress() - FixupAddress + E.getAddend();
    endian::write64le(FixupPtr, static_cast<uint64_t>(Value));
    break;
  }
  case Delta32: {
    int64_t Value = E.getTarget().getAddress() - FixupAddress + E.getAddend();
    if (!isInt<32>(Value))
      return makeTargetOutOfRangeError(G, B, E);
    endian::write32le(FixupPtr, static_cast<uint32_t>(Value));
    break;
  }
  case NegDelta32: {
    int64_t Value = FixupAddress - E.getTarget().getAddress() + E.getAddend();
    if (!isInt<32>(Value))
      return makeTargetOutOfRangeError(G, B, E);
    endian::write32le(FixupPtr, static_cast<uint32_t>(Value));
    break;
  }
  case Branch21PCRel:
  case BranchPCRel21ToPLT: {
    int64_t Value =
        E.getTarget().getAddress() - (FixupAddress + 4) + E.getAddend();
    if (Value & 3)
      return make_error<JITLinkError>("Branch target is not instruction "
                                      "aligned at " +
                                      formatv("{0:x}", FixupAddress));
    Value >>= 2;
    if (!isInt<21>(Value))
      return makeTargetOutOfRangeError(G, B, E);
    uint32_t Insn = endian::read32le(FixupPtr);
    Insn = (Insn & 0xffe00000) | (static_cast<uint32_t>(Value) & 0x1fffff);
    endian::write32le(FixupPtr, Insn);
    break;
  }
  case Hint14PCRel: {
    // A wrong hint costs a mispredicted jump and nothing else, so an
    // out-of-range value is simply truncated.
    int64_t Value =
        (E.getTarget().getAddress() - (FixupAddress + 4) + E.getAddend()) >> 2;
    uint32_t Insn = endian::read32le(FixupPtr);
    Insn = (Insn & 0xffffc000) | (static_cast<uint32_t>(Value) & 0x3fff);
    endian::write32le(FixupPtr, Insn);
    break;
  }
  case GPRelHigh16:
  case GPRelLow16:
  case GPRel16:
  case GPRel32: {
    int64_t Value =
        E.getTarget().getAddress() - getGPForGOT(GOTSymbol) + E.getAddend();
    if (E.getKind() == GPRel32) {
      if (!isInt<32>(Value))
        return makeTargetOutOfRangeError(G, B, E);
      endian::write32le(FixupPtr, static_cast<uint32_t>(Value));
      break;
    }
    int64_t Half;
    if (E.getKind() == GPRelHigh16)
      // The low half is a signed field, so its sign bit is a carry into this
      // one.
      Half = (Value + 0x8000) >> 16;
    else if (E.getKind() == GPRelLow16)
      Half = static_cast<int16_t>(Value & 0xffff);
    else
      Half = Value;
    if (!isInt<16>(Half))
      return makeTargetOutOfRangeError(G, B, E);
    uint32_t Insn = endian::read32le(FixupPtr);
    Insn = (Insn & 0xffff0000) | (static_cast<uint32_t>(Half) & 0xffff);
    endian::write32le(FixupPtr, Insn);
    break;
  }
  case GPDisp: {
    // The addend gives the distance from the ldah this fixup sits on to its
    // lda, which is normally the next instruction.
    int64_t Value = getGPForGOT(GOTSymbol) - FixupAddress;
    int64_t High = (Value + 0x8000) >> 16;
    if (!isInt<16>(High))
      return makeTargetOutOfRangeError(G, B, E);
    uint32_t Ldah = endian::read32le(FixupPtr);
    Ldah = (Ldah & 0xffff0000) | (static_cast<uint32_t>(High) & 0xffff);
    endian::write32le(FixupPtr, Ldah);

    // Block::addEdge bounds-checks the offset, but nothing checks this second
    // write, which the addend alone places.
    int64_t LdaOff = static_cast<int64_t>(E.getOffset()) + E.getAddend();
    if (LdaOff < 0 || static_cast<uint64_t>(LdaOff) + 4 > B.getSize())
      return make_error<JITLinkError>(
          "In graph " + G.getName() + ", section " + B.getSection().getName() +
          ", gpdisp lda at offset " + formatv("{0}", LdaOff) +
          " is outside the block");
    char *LdaPtr = BlockWorkingMem + LdaOff;
    uint32_t Lda = endian::read32le(LdaPtr);
    Lda = (Lda & 0xffff0000) | (static_cast<uint32_t>(Value) & 0xffff);
    endian::write32le(LdaPtr, Lda);
    break;
  }
  default:
    return make_error<JITLinkError>(
        "In graph " + G.getName() + ", section " + B.getSection().getName() +
        " unsupported edge kind " + getEdgeKindName(E.getKind()));
  }

  return Error::success();
}

/// A GOT entry is a single quadword holding the address of its target.
constexpr uint64_t PointerSize = 8;
extern const char NullPointerContent[PointerSize];

inline Symbol &createAnonymousPointer(LinkGraph &G, Section &PointerSection,
                                      Symbol *InitialTarget = nullptr) {
  auto &B = G.createContentBlock(PointerSection, NullPointerContent,
                                 orc::ExecutorAddr(), 8, 0);
  if (InitialTarget)
    B.addEdge(Pointer64, 0, *InitialTarget, 0);
  return G.addAnonymousSymbol(B, 0, PointerSize, false, false);
}

/// A stub jumps to the address in its GOT entry.  Alpha cannot form a
/// PC-relative address, so the stub takes the address of its own second
/// instruction with a branch, reads the address of the GOT entry from the
/// quadword that follows it, and loads the target through that.
///
///   br  $27, .+4
///   ldq $27, 12($27)
///   ldq $27, 0($27)
///   jmp $31, ($27), 0
///   .quad got_entry
constexpr size_t StubEntrySize = 24;
extern const uint8_t StubContent[StubEntrySize];

inline Symbol &createAnonymousPointerJumpStub(LinkGraph &G,
                                              Section &StubSection,
                                              Symbol &PointerTarget) {
  auto &B = G.createContentBlock(
      StubSection,
      ArrayRef<char>(reinterpret_cast<const char *>(StubContent),
                     StubEntrySize),
      orc::ExecutorAddr(), 8, 0);
  B.addEdge(Pointer64, 16, PointerTarget, 0);
  return G.addAnonymousSymbol(B, 0, StubEntrySize, true, false);
}

/// Global Offset Table Builder.
class GOTTableManager : public TableManager<GOTTableManager> {
public:
  static StringRef getSectionName() { return "$__GOT"; }

  bool visitEdge(LinkGraph &G, Block *B, Edge &E) {
    if (E.getKind() != RequestGOTAndTransformToGPRel16)
      return false;
    E.setTarget(getEntryForTarget(G, E.getTarget()));
    E.setKind(GPRel16);
    return true;
  }

  Symbol &createEntry(LinkGraph &G, Symbol &Target) {
    return createAnonymousPointer(G, getGOTSection(G), &Target);
  }

private:
  Section &getGOTSection(LinkGraph &G) {
    if (!GOTSection)
      GOTSection = &G.createSection(getSectionName(),
                                    orc::MemProt::Read | orc::MemProt::Write);
    return *GOTSection;
  }

  Section *GOTSection = nullptr;
};

/// Procedure Linkage Table Builder.
class PLTTableManager : public TableManager<PLTTableManager> {
public:
  PLTTableManager(GOTTableManager &GOT) : GOT(GOT) {}

  static StringRef getSectionName() { return "$__STUBS"; }

  bool visitEdge(LinkGraph &G, Block *B, Edge &E) {
    if (E.getKind() == BranchPCRel21ToPLT && !E.getTarget().isDefined()) {
      E.setKind(Branch21PCRel);
      E.setTarget(getEntryForTarget(G, E.getTarget()));
      return true;
    }
    return false;
  }

  Symbol &createEntry(LinkGraph &G, Symbol &Target) {
    return createAnonymousPointerJumpStub(G, getStubsSection(G),
                                          GOT.getEntryForTarget(G, Target));
  }

private:
  Section &getStubsSection(LinkGraph &G) {
    if (!StubsSection)
      StubsSection = &G.createSection(getSectionName(),
                                      orc::MemProt::Read | orc::MemProt::Exec);
    return *StubsSection;
  }

  GOTTableManager &GOT;
  Section *StubsSection = nullptr;
};

} // namespace llvm::jitlink::alpha

#endif // LLVM_EXECUTIONENGINE_JITLINK_ALPHA_H
