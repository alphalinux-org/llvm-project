//===-- AlphaVerifyInvariants.cpp - Check invariants no test can see ------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Three properties of Alpha code that a FileCheck test cannot observe, checked
// over the final machine function so that every function anything compiles
// becomes a test of all three.
//
//   1. No memory access, call or inline assembly appears between a load locked
//      and its store conditional.  The Alpha Architecture Handbook (5.5.2)
//      only guarantees forward progress for a sequence with none, and on real
//      hardware an intervening access clears the lock flag, so the store
//      conditional fails every time round.  qemu does not model the lock flag,
//      so no test running under emulation can see this: the first time it was
//      found, it was as a hang on a 21264.
//
//   2. No floating-point register is touched under -mno-fp-regs.  The whole
//      point of that option is that the kernel need not save the FP file for
//      the process; one FP instruction anywhere makes the generated code
//      wrong in a way that nothing in the output looks wrong.
//
//   3. The global pointer is established before anything reads it, and is
//      re-established after any call that clobbers it without restoring it.
//      $29 is a reserved register, so the machine verifier's liveness rules
//      do not apply to it and nothing else checks this.
//
// The precedent is CompleteModel, which makes a missing InstRW a TableGen
// error and so keeps scheduling-model completeness true by construction rather
// than by review.
//
// The invariant checked outside this pass -- that no instruction requiring a
// subtarget feature is emitted for a subtarget without it -- is in
// AlphaAsmPrinter, where TableGen already generates the predicate table
// (Alpha_MC::verifyInstructionPredicates).
//
// The pass is on by default in any build with assertions and can be turned on
// or off anywhere with -alpha-check-invariants.
//
//===----------------------------------------------------------------------===//

#include "Alpha.h"
#include "AlphaInstrInfo.h"
#include "AlphaMachineFunctionInfo.h"
#include "AlphaSubtarget.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/DenseSet.h"
#include "llvm/ADT/MapVector.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

#define ALPHA_VERIFY_INVARIANTS_NAME "Alpha machine invariant verifier"

// On by default wherever assertions are, which is what turns every existing
// test into a test of all three invariants.  Three linear walks over the
// machine function cost nothing next to the rest of an assertions build.
#ifdef NDEBUG
static constexpr bool DefaultEnabled = false;
#else
static constexpr bool DefaultEnabled = true;
#endif

static cl::opt<bool> EnableVerify(
    // Not "alpha-verify-invariants": that is the pass's own name, and `opt`
    // registers a command-line option for every pass name, so the two collide
    // and every tool that links this target aborts at startup.
    "alpha-check-invariants", cl::Hidden, cl::init(DefaultEnabled),
    cl::desc("Check the Alpha invariants no test can observe: the load "
             "locked / store conditional window, -mno-fp-regs, and the "
             "global pointer"));

namespace {

class AlphaVerifyInvariants : public MachineFunctionPass {
public:
  static char ID;
  AlphaVerifyInvariants() : MachineFunctionPass(ID) {}

  bool runOnMachineFunction(MachineFunction &MF) override;

  StringRef getPassName() const override {
    return ALPHA_VERIFY_INVARIANTS_NAME;
  }

  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.setPreservesAll();
    MachineFunctionPass::getAnalysisUsage(AU);
  }

private:
  const AlphaSubtarget *STI = nullptr;

  void fail(const MachineFunction &MF, const MachineInstr &MI, StringRef Id,
            const Twine &What) const;
  void checkLLSCWindow(MachineFunction &MF) const;
  void checkNoFPRegs(MachineFunction &MF) const;
  void checkGlobalPointer(MachineFunction &MF) const;
};

} // end anonymous namespace

void AlphaVerifyInvariants::fail(const MachineFunction &MF,
                                 const MachineInstr &MI, StringRef Id,
                                 const Twine &What) const {
  std::string Msg;
  raw_string_ostream OS(Msg);
  OS << "Alpha invariant " << Id << " violated in function '" << MF.getName()
     << "': " << What << "\n  ";
  MI.print(OS, /*IsStandalone=*/false);
  report_fatal_error(StringRef(Msg), /*GenCrashDiag=*/false);
}

//===----------------------------------------------------------------------===//
// 1. The load locked / store conditional window.

static bool isLoadLocked(const MachineInstr &MI) {
  return MI.getOpcode() == Alpha::LDL_L || MI.getOpcode() == Alpha::LDQ_L;
}

static bool isStoreConditional(const MachineInstr &MI) {
  return MI.getOpcode() == Alpha::STL_C || MI.getOpcode() == Alpha::STQ_C;
}

void AlphaVerifyInvariants::checkLLSCWindow(MachineFunction &MF) const {
  for (MachineBasicBlock &MBB : MF) {
    for (MachineInstr &LL : MBB) {
      if (!isLoadLocked(LL))
        continue;

      // The window is every instruction on a path from this load locked to a
      // store conditional that closes it.  Two things are deliberately not in
      // it.  A path that reaches another load locked first has abandoned this
      // reservation -- the compare-and-swap expansion exits its inner loop
      // that way and then does stack traffic, which is harmless.  And a path
      // that leaves the loop for good never reaches a store conditional at
      // all.  So the region is the *intersection* of what the load locked
      // reaches and what reaches a store conditional.
      //
      // A segment is a block entered at a given point, scanned to the first
      // load locked or store conditional in it.  Blocks can be entered at two
      // different points -- the loop header from outside and from the back
      // edge -- so segments are keyed by both.
      using Seg = std::pair<MachineBasicBlock *, MachineInstr *>;
      struct SegInfo {
        MachineBasicBlock::iterator Begin, End;
        bool Closes = false;                 // ends at a store conditional
        bool FallsOut = false;               // ends at the block's end
        SmallVector<Seg, 4> Succs;
      };
      MapVector<Seg, SegInfo> Segs;
      SmallVector<Seg, 8> Work;

      auto key = [](MachineBasicBlock *BB, MachineBasicBlock::iterator It) {
        return Seg{BB, It == BB->end() ? nullptr : &*It};
      };
      auto push = [&](MachineBasicBlock *BB, MachineBasicBlock::iterator It) {
        Seg K = key(BB, It);
        if (Segs.count(K))
          return K;
        SegInfo &SI = Segs[K];
        SI.Begin = It;
        for (; It != BB->end(); ++It) {
          if (isStoreConditional(*It)) {
            SI.Closes = true;
            break;
          }
          if (isLoadLocked(*It))
            break; // this reservation has been abandoned
        }
        SI.End = It;
        SI.FallsOut = !SI.Closes && It == BB->end();
        Work.push_back(K);
        return K;
      };

      push(&MBB, std::next(LL.getIterator()));
      while (!Work.empty()) {
        Seg K = Work.pop_back_val();
        if (!Segs[K].FallsOut)
          continue;
        MachineBasicBlock *BB = K.first;
        SmallVector<Seg, 4> Succs;
        for (MachineBasicBlock *S : BB->successors())
          Succs.push_back(push(S, S->begin()));
        Segs[K].Succs = std::move(Succs);
      }

      // Which segments can still reach a store conditional.
      DenseSet<Seg> Reaches;
      bool Changed = true;
      while (Changed) {
        Changed = false;
        for (auto &KV : Segs) {
          if (Reaches.count(KV.first))
            continue;
          bool R = KV.second.Closes;
          for (const Seg &S : KV.second.Succs)
            R |= Reaches.count(S) != 0;
          if (R) {
            Reaches.insert(KV.first);
            Changed = true;
          }
        }
      }

      for (auto &KV : Segs) {
        if (!Reaches.count(KV.first))
          continue;
        for (auto It = KV.second.Begin; It != KV.second.End; ++It) {
          MachineInstr &MI = *It;
          if (MI.isCall())
            fail(MF, MI, "B1",
                 "a call appears between a load locked and its store "
                 "conditional; the architecture does not guarantee forward "
                 "progress and the reservation is lost on real hardware");
          if (MI.isInlineAsm())
            fail(MF, MI, "B1",
                 "inline assembly appears between a load locked and its "
                 "store conditional, and its contents cannot be checked");
          if (MI.mayLoadOrStore())
            fail(MF, MI, "B1",
                 "a memory access appears between a load locked and its "
                 "store conditional; the reservation is lost on real "
                 "hardware and the loop spins forever");
        }
      }
    }
  }
}

//===----------------------------------------------------------------------===//
// 2. -mno-fp-regs.

void AlphaVerifyInvariants::checkNoFPRegs(MachineFunction &MF) const {
  if (!STI->hasNoFPRegs())
    return;
  const TargetRegisterInfo &TRI = *STI->getRegisterInfo();
  const auto &F4 = Alpha::F4RCRegClass;
  const auto &F8 = Alpha::F8RCRegClass;
  for (MachineBasicBlock &MBB : MF) {
    for (MachineInstr &MI : MBB) {
      if (MI.isDebugInstr())
        continue;
      for (const MachineOperand &MO : MI.operands()) {
        if (!MO.isReg() || !MO.getReg())
          continue;
        Register R = MO.getReg();
        if (!R.isPhysical())
          continue;
        if (F4.contains(R) || F8.contains(R)) {
          std::string Name;
          raw_string_ostream OS(Name);
          OS << printReg(R, &TRI);
          fail(MF, MI, "B3",
               Twine("floating-point register ") + Name +
                   " is touched under -mno-fp-regs");
        }
      }
    }
  }
}

//===----------------------------------------------------------------------===//
// 3. The global pointer.
//
// $29 is reserved, so its liveness is not tracked and the machine verifier
// says nothing about it.  This walks the function's blocks to a fixed point,
// carrying one bit -- whether $29 holds this function's global pointer -- and
// reports any read taken where it does not.

void AlphaVerifyInvariants::checkGlobalPointer(MachineFunction &MF) const {
  const AlphaMachineFunctionInfo *AFI = MF.getInfo<AlphaMachineFunctionInfo>();
  if (MF.empty())
    return;

  // The prologue's ldgp is written by the AsmPrinter, not by an instruction,
  // so the entry block's state comes from usesGP() rather than from a def.
  const bool EntryEstablished = AFI->usesGP();

  // "Established on entry to this block" is a must-property: it holds only if
  // it holds along every path.  A must-analysis has to start optimistic and
  // fall, or a loop header would be reported the moment its back edge has not
  // been visited yet -- which is exactly how the first version of this check
  // mis-reported a call inside a loop.
  DenseMap<const MachineBasicBlock *, bool> In, Out;
  for (const MachineBasicBlock &MBB : MF) {
    In[&MBB] = true;
    Out[&MBB] = true;
  }

  // Not every call reaches a routine that follows the standard calling
  // sequence.  Three of them run on the caller's global pointer and so leave
  // $29 holding it, which is why none of them emits an ldgp afterwards (see
  // their comments in AlphaInstrInfo.td):
  //
  //   DIVCALL   the division millicode -- __divq and its three siblings,
  //             entered through $23 -- preserves every register but $23-$25,
  //             $27 and $28.
  //   BSR       the machine outliner's call.  The outlined function cannot
  //             touch $29: getOutliningTypeImpl rules out every instruction
  //             that reads or writes it.  It returns through $23.
  //   CALLbsr   the -msmall-text direct call.  It loads no procedure value,
  //             and its R_ALPHA_BRSGP relocation aims the branch past the
  //             callee's own gp prologue precisely so the callee inherits
  //             ours.
  //
  // A call that preserves $29 must not clear the bit.  Clearing it reports the
  // next read of the global pointer as a read of a dead one and refuses to
  // compile a correct function -- which it did for the second of two divisions
  // in one function, for a literal load after an outlined call, and for a
  // -msmall-text call followed by any access to a global.
  //
  // Defaulting to "clobbers" is deliberate: a call this does not know about is
  // reported rather than trusted.  But it means a new call pseudo that runs on
  // the caller's gp has to be added here, and three times now that has been
  // noticed only when a build stopped.
  auto callPreservesGP = [](const MachineInstr &MI) {
    switch (MI.getOpcode()) {
    case Alpha::DIVCALL:
    case Alpha::BSR:
    case Alpha::CALLbsr:
      return true;
    default:
      return false;
    }
  };

  // Within a block: a def of $29 establishes it -- a call pseudo that lists
  // $29 among its defs carries the ldgp reload in its expansion -- and a call
  // that neither defines nor preserves it leaves it holding the callee's.
  auto blockOut = [&](MachineBasicBlock &MBB, bool Established) {
    for (MachineInstr &MI : MBB) {
      if (MI.isDebugInstr())
        continue;
      if (MI.definesRegister(Alpha::R29, /*TRI=*/nullptr))
        Established = true;
      else if (MI.isCall() && !callPreservesGP(MI))
        Established = false;
    }
    return Established;
  };

  bool Changed = true;
  while (Changed) {
    Changed = false;
    for (MachineBasicBlock &MBB : MF) {
      bool NewIn;
      if (&MBB == &MF.front()) {
        NewIn = EntryEstablished;
      } else {
        NewIn = !MBB.pred_empty();
        for (const MachineBasicBlock *P : MBB.predecessors())
          NewIn &= Out[P];
      }
      bool NewOut = blockOut(MBB, NewIn);
      if (NewIn != In[&MBB] || NewOut != Out[&MBB]) {
        In[&MBB] = NewIn;
        Out[&MBB] = NewOut;
        Changed = true;
      }
    }
  }

  // Only now, with the states settled, report.
  for (MachineBasicBlock &MBB : MF) {
    bool Established = In[&MBB];
    for (MachineInstr &MI : MBB) {
      if (MI.isDebugInstr())
        continue;
      if (MI.readsRegister(Alpha::R29, /*TRI=*/nullptr) && !Established)
        fail(MF, MI, "B4",
             "$29 is read where it does not hold this function's global "
             "pointer -- either the prologue did not establish it or a call "
             "clobbered it without a reload");
      if (MI.definesRegister(Alpha::R29, /*TRI=*/nullptr))
        Established = true;
      else if (MI.isCall() && !callPreservesGP(MI))
        Established = false;
    }
  }
}

//===----------------------------------------------------------------------===//

bool AlphaVerifyInvariants::runOnMachineFunction(MachineFunction &MF) {
  if (!EnableVerify)
    return false;
  STI = &MF.getSubtarget<AlphaSubtarget>();
  checkLLSCWindow(MF);
  checkNoFPRegs(MF);
  checkGlobalPointer(MF);
  return false;
}

char AlphaVerifyInvariants::ID = 0;

INITIALIZE_PASS(AlphaVerifyInvariants, "alpha-verify-invariants",
                ALPHA_VERIFY_INVARIANTS_NAME, false, true)

FunctionPass *llvm::createAlphaVerifyInvariants() {
  return new AlphaVerifyInvariants();
}
