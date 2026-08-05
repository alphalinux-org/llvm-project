//===----- ELF_alpha.cpp - JIT linker implementation for ELF/alpha --------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// ELF/alpha jit-link implementation.
//
//===----------------------------------------------------------------------===//

#include "llvm/ExecutionEngine/JITLink/ELF_alpha.h"
#include "llvm/ExecutionEngine/JITLink/DWARFRecordSectionSplitter.h"
#include "llvm/ExecutionEngine/JITLink/alpha.h"
#include "llvm/Object/ELFObjectFile.h"

#include "DefineExternalSectionStartAndEndSymbols.h"
#include "EHFrameSupportImpl.h"
#include "ELFLinkGraphBuilder.h"
#include "JITLinkGeneric.h"

#define DEBUG_TYPE "jitlink"

using namespace llvm;
using namespace llvm::jitlink;

namespace {

// st_other bits 7 and 3 say how a callee sets up its own global pointer, and
// so whether a !samegp caller may skip the two-instruction gp load.
enum { STO_ALPHA_NOPV = 0x80, STO_ALPHA_STD_GPLOAD = 0x88 };

constexpr StringRef ELFGOTSymbolName = "_GLOBAL_OFFSET_TABLE_";

Error buildTables_ELF_alpha(LinkGraph &G) {
  LLVM_DEBUG(dbgs() << "Visiting edges in graph:\n");

  alpha::GOTTableManager GOT;
  alpha::PLTTableManager PLT(GOT);
  visitExistingEdges(G, GOT, PLT);
  return Error::success();
}

} // namespace

namespace llvm::jitlink {

class ELFJITLinker_alpha : public JITLinker<ELFJITLinker_alpha> {
  friend class JITLinker<ELFJITLinker_alpha>;

public:
  ELFJITLinker_alpha(std::unique_ptr<JITLinkContext> Ctx,
                     std::unique_ptr<LinkGraph> G, PassConfiguration PassConfig)
      : JITLinker(std::move(Ctx), std::move(G), std::move(PassConfig)) {
    getPassConfig().PostAllocationPasses.push_back(
        [this](LinkGraph &G) { return getOrCreateGOTSymbol(G); });
  }

private:
  Symbol *GOTSymbol = nullptr;

  Error applyFixup(LinkGraph &G, Block &B, const Edge &E) const {
    return alpha::applyFixup(G, B, E, GOTSymbol);
  }

  // Every global-pointer-relative fixup measures from one place, so the graph
  // needs a symbol for the GOT even when nothing in the object named it.
  Error getOrCreateGOTSymbol(LinkGraph &G) {
    auto DefineExternalGOTSymbolIfPresent =
        createDefineExternalSectionStartAndEndSymbolsPass(
            [&](LinkGraph &LG, Symbol &Sym) -> SectionRangeSymbolDesc {
              if (Sym.getName() != nullptr &&
                  *Sym.getName() == ELFGOTSymbolName)
                if (auto *GOTSection = G.findSectionByName(
                        alpha::GOTTableManager::getSectionName())) {
                  GOTSymbol = &Sym;
                  return {*GOTSection, true};
                }
              return {};
            });

    if (auto Err = DefineExternalGOTSymbolIfPresent(G))
      return Err;

    if (GOTSymbol)
      return Error::success();

    if (auto *GOTSection =
            G.findSectionByName(alpha::GOTTableManager::getSectionName())) {
      for (auto *Sym : GOTSection->symbols())
        if (Sym->getName() != nullptr && *Sym->getName() == ELFGOTSymbolName) {
          GOTSymbol = Sym;
          return Error::success();
        }

      SectionRange SR(*GOTSection);
      if (SR.empty())
        GOTSymbol =
            &G.addAbsoluteSymbol(ELFGOTSymbolName, orc::ExecutorAddr(), 0,
                                 Linkage::Strong, Scope::Local, true);
      else
        GOTSymbol =
            &G.addDefinedSymbol(*SR.getFirstBlock(), 0, ELFGOTSymbolName, 0,
                                Linkage::Strong, Scope::Local, false, true);
      return Error::success();
    }

    // No GOT section: a global-pointer-relative fixup can still appear, so
    // anchor the symbol somewhere in the graph.
    for (auto *Sym : G.external_symbols()) {
      if (Sym->getName() != nullptr && *Sym->getName() == ELFGOTSymbolName) {
        auto Blocks = G.blocks();
        if (!Blocks.empty()) {
          G.makeAbsolute(*Sym, (*Blocks.begin())->getAddress());
          GOTSymbol = Sym;
          return Error::success();
        }
      }
    }

    auto Blocks = G.blocks();
    if (!Blocks.empty())
      GOTSymbol = &G.addAbsoluteSymbol(ELFGOTSymbolName,
                                       (*Blocks.begin())->getAddress(), 0,
                                       Linkage::Strong, Scope::Local, true);

    // A graph with no blocks at all has nothing to anchor the symbol to, but
    // returning success with none defined is worse than anchoring it at zero:
    // applyFixup reads through it unconditionally, so a gp-relative fixup would
    // assert in a build with assertions and dereference null without them.
    if (!GOTSymbol)
      GOTSymbol = &G.addAbsoluteSymbol(ELFGOTSymbolName, orc::ExecutorAddr(), 0,
                                       Linkage::Strong, Scope::Local, true);

    return Error::success();
  }
};

class ELFLinkGraphBuilder_alpha : public ELFLinkGraphBuilder<object::ELF64LE> {
private:
  using ELFT = object::ELF64LE;
  using Base = ELFLinkGraphBuilder<ELFT>;
  using Base::G;

  Error addRelocations() override {
    LLVM_DEBUG(dbgs() << "Processing relocations:\n");

    using Self = ELFLinkGraphBuilder_alpha;
    for (const auto &RelSect : Base::Sections) {
      if (RelSect.sh_type == ELF::SHT_REL)
        return make_error<StringError>("No SHT_REL in valid " +
                                           G->getTargetTriple().getArchName() +
                                           " ELF object files",
                                       inconvertibleErrorCode());

      if (Error Err = Base::forEachRelaRelocation(RelSect, this,
                                                  &Self::addSingleRelocation))
        return Err;
    }

    return Error::success();
  }

  Error addSingleRelocation(const typename ELFT::Rela &Rel,
                            const typename ELFT::Shdr &FixupSect,
                            Block &BlockToFix) {
    uint32_t ELFReloc = Rel.getType(false);

    // These carry no fixup of their own: a hint is only a branch prediction
    // and a lituse only marks an instruction the linker could relax.
    if (ELFReloc == ELF::R_ALPHA_NONE || ELFReloc == ELF::R_ALPHA_LITUSE)
      return Error::success();

    uint32_t SymbolIndex = Rel.getSymbol(false);
    auto ObjSymbol = Base::Obj.getRelocationSymbol(Rel, Base::SymTabSec);
    if (!ObjSymbol)
      return ObjSymbol.takeError();

    Symbol *GraphSymbol = Base::getGraphSymbol(SymbolIndex);
    if (!GraphSymbol)
      return make_error<StringError>(
          formatv("Could not find symbol at given index, did you add it to "
                  "JITSymbolTable? index: {0}, shndx: {1} Size of table: {2}",
                  SymbolIndex, (*ObjSymbol)->st_shndx,
                  Base::GraphSymbols.size()),
          inconvertibleErrorCode());

    int64_t Addend = Rel.r_addend;
    Edge::Kind Kind = Edge::Invalid;

    switch (ELFReloc) {
    case ELF::R_ALPHA_REFQUAD:
      Kind = alpha::Pointer64;
      break;
    case ELF::R_ALPHA_REFLONG:
      Kind = alpha::Pointer32;
      break;
    case ELF::R_ALPHA_SREL64:
      Kind = alpha::Delta64;
      break;
    case ELF::R_ALPHA_SREL32:
      Kind = alpha::Delta32;
      break;
    case ELF::R_ALPHA_BRADDR:
      Kind = alpha::BranchPCRel21ToPLT;
      break;
    case ELF::R_ALPHA_BRSGP:
      // A !samegp call leaves $27 unset and lands past the callee's gp-load
      // prologue, whose length the callee advertises in st_other. Aiming a
      // stub at it would run the stub from the middle, and branching past a
      // prologue a callee never announced leaves gp wrong, so neither is
      // guessed at -- which is how bfd and lld treat this relocation too.
      switch ((*ObjSymbol)->st_other & STO_ALPHA_STD_GPLOAD) {
      case STO_ALPHA_NOPV:
        break;
      case STO_ALPHA_STD_GPLOAD:
        Addend += 8;
        break;
      default:
        return make_error<JITLinkError>(
            "!samegp relocation against symbol without .prologue: " +
            *GraphSymbol->getName());
      }
      Kind = alpha::Branch21PCRel;
      break;
    case ELF::R_ALPHA_HINT:
      Kind = alpha::Hint14PCRel;
      break;
    case ELF::R_ALPHA_GPRELHIGH:
      Kind = alpha::GPRelHigh16;
      break;
    case ELF::R_ALPHA_GPRELLOW:
      Kind = alpha::GPRelLow16;
      break;
    case ELF::R_ALPHA_GPREL16:
      Kind = alpha::GPRel16;
      break;
    case ELF::R_ALPHA_GPREL32:
      Kind = alpha::GPRel32;
      break;
    case ELF::R_ALPHA_GPDISP:
      Kind = alpha::GPDisp;
      break;
    case ELF::R_ALPHA_LITERAL:
      Kind = alpha::RequestGOTAndTransformToGPRel16;
      break;
    default:
      return make_error<JITLinkError>(
          "In " + G->getName() + ": Unsupported alpha relocation type " +
          object::getELFRelocationTypeName(ELF::EM_ALPHA, ELFReloc));
    }

    auto FixupAddress = orc::ExecutorAddr(FixupSect.sh_addr) + Rel.r_offset;
    Edge::OffsetT Offset = FixupAddress - BlockToFix.getAddress();
    Edge GE(Kind, Offset, *GraphSymbol, Addend);
    LLVM_DEBUG({
      dbgs() << "    ";
      printEdge(dbgs(), BlockToFix, GE, alpha::getEdgeKindName(Kind));
      dbgs() << "\n";
    });

    BlockToFix.addEdge(std::move(GE));
    return Error::success();
  }

public:
  ELFLinkGraphBuilder_alpha(StringRef FileName,
                            const object::ELFFile<ELFT> &Obj,
                            std::shared_ptr<orc::SymbolStringPool> SSP,
                            Triple TT, SubtargetFeatures Features)
      : ELFLinkGraphBuilder<ELFT>(Obj, std::move(SSP), std::move(TT),
                                  std::move(Features), FileName,
                                  alpha::getEdgeKindName) {}
};

Expected<std::unique_ptr<LinkGraph>>
createLinkGraphFromELFObject_alpha(MemoryBufferRef ObjectBuffer,
                                   std::shared_ptr<orc::SymbolStringPool> SSP) {
  LLVM_DEBUG({
    dbgs() << "Building jitlink graph for new input "
           << ObjectBuffer.getBufferIdentifier() << "...\n";
  });

  auto ELFObj = object::ObjectFile::createELFObjectFile(ObjectBuffer);
  if (!ELFObj)
    return ELFObj.takeError();

  auto Features = (*ELFObj)->getFeatures();
  if (!Features)
    return Features.takeError();

  assert((*ELFObj)->getArch() == Triple::alpha && "Only alpha is supported");

  auto &ELFObjFile = cast<object::ELFObjectFile<object::ELF64LE>>(**ELFObj);
  return ELFLinkGraphBuilder_alpha(
             (*ELFObj)->getFileName(), ELFObjFile.getELFFile(), std::move(SSP),
             (*ELFObj)->makeTriple(), std::move(*Features))
      .buildGraph();
}

void link_ELF_alpha(std::unique_ptr<LinkGraph> G,
                    std::unique_ptr<JITLinkContext> Ctx) {
  PassConfiguration Config;
  const Triple &TT = G->getTargetTriple();
  if (Ctx->shouldAddDefaultTargetPasses(TT)) {
    // Add eh-frame passes.
    Config.PrePrunePasses.push_back(DWARFRecordSectionSplitter(".eh_frame"));
    Config.PrePrunePasses.push_back(EHFrameEdgeFixer(
        ".eh_frame", G->getPointerSize(), alpha::Pointer32, alpha::Pointer64,
        alpha::Delta32, alpha::Delta64, alpha::NegDelta32));
    Config.PrePrunePasses.push_back(EHFrameNullTerminator(".eh_frame"));

    // Add a mark-live pass.
    if (auto MarkLive = Ctx->getMarkLivePass(TT))
      Config.PrePrunePasses.push_back(std::move(MarkLive));
    else
      Config.PrePrunePasses.push_back(markAllSymbolsLive);

    // Add an in-place GOT/stubs build pass.
    Config.PostPrunePasses.push_back(buildTables_ELF_alpha);

    // Resolve any external section start / end symbols.
    Config.PostAllocationPasses.push_back(
        createDefineExternalSectionStartAndEndSymbolsPass(
            identifyELFSectionStartAndEndSymbols));
  }

  if (auto Err = Ctx->modifyPassConfig(*G, Config))
    return Ctx->notifyFailed(std::move(Err));

  ELFJITLinker_alpha::link(std::move(Ctx), std::move(G), std::move(Config));
}

} // namespace llvm::jitlink
