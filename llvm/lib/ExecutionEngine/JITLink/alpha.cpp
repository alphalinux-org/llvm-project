//===----- alpha.cpp - Generic JITLink alpha edge kinds, utilities --------===//
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

#include "llvm/ExecutionEngine/JITLink/alpha.h"

#define DEBUG_TYPE "jitlink"

namespace llvm::jitlink::alpha {

const char NullPointerContent[PointerSize] = {0x00, 0x00, 0x00, 0x00,
                                              0x00, 0x00, 0x00, 0x00};

// br  $27, .+4         ; $27 = address of the next instruction
// ldq $27, 12($27)     ; load the address of the GOT entry stored below
// ldq $27, 0($27)      ; load the target out of the GOT entry
// jmp $31, ($27), 0
// .quad got_entry
const uint8_t StubContent[StubEntrySize] = {
    0x00, 0x00, 0x60, 0xc3, // br $27, .+4
    0x0c, 0x00, 0x7b, 0xa7, // ldq $27, 12($27)
    0x00, 0x00, 0x7b, 0xa7, // ldq $27, 0($27)
    0x00, 0x00, 0xfb, 0x6b, // jmp $31, ($27), 0
    0x00, 0x00, 0x00, 0x00, // the GOT entry address, from a Pointer64 edge
    0x00, 0x00, 0x00, 0x00,
};

const char *getEdgeKindName(Edge::Kind K) {
  switch (K) {
  case Pointer64:
    return "Pointer64";
  case Pointer32:
    return "Pointer32";
  case Delta64:
    return "Delta64";
  case Delta32:
    return "Delta32";
  case NegDelta32:
    return "NegDelta32";
  case Branch21PCRel:
    return "Branch21PCRel";
  case Hint14PCRel:
    return "Hint14PCRel";
  case GPRelHigh16:
    return "GPRelHigh16";
  case GPRelLow16:
    return "GPRelLow16";
  case GPRel16:
    return "GPRel16";
  case GPRel32:
    return "GPRel32";
  case GPDisp:
    return "GPDisp";
  case RequestGOTAndTransformToGPRel16:
    return "RequestGOTAndTransformToGPRel16";
  case BranchPCRel21ToPLT:
    return "BranchPCRel21ToPLT";
  }
  return getGenericEdgeKindName(K);
}

} // namespace llvm::jitlink::alpha
