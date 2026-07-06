# Review: commits 131–160 (alpha-triple-0bf3638)

## Summary

Thirty commits covering the TTI cost model, tail calls, several DAG combines,
the machine combiner and outliner, branch relaxation, clang driver/target
plumbing, and a large block of integrated-assembler work for hand-written
kernel assembly.  The code is generally well shaped and idiomatic for an LLVM
backend; the problems cluster in four areas:

1. **Tests that cannot fail for the thing they claim to test.**
   `blockaddress.ll` (4d135b7) never exercises `LowerBlockAddress`;
   `ldgp-first.ll` (0ff321c) never checks that the ldgp is *first*;
   `directives.s` (42b87b7) never checks the 2^N padding it exists for;
   `remat-const.ll` (3e1b896) has no negative check for the spill it prevents.
2. **A fix that must be squashed backwards.** 6694f35 ("Prefer a sign extension
   only from i32") reverts most of acfc9ca and says outright that acfc9ca's test
   "passed with the change reverted".  It also carries internal tracker IDs
   ("Fixes ALPHA-021 ... ALPHA-T02") that must not go upstream, and leaves a
   stale, now-contradictory comment in place above the new one.
3. **Commit messages that describe code that is not in the commit.** c864fb8
   claims to add the a/b/c/v register constraint letters (it does not);
   156e4bd's first paragraph describes `initFeatureMap`, which already existed
   in the parent; bd89c4b claims "$rN register spellings" that appear nowhere.
4. **Two oversized/mixed MC commits.** bd89c4b (7 unrelated bullet groups) and
   8b354e4 (which rewrites the `.ent`/`.prologue`/`.end` handling bd89c4b added
   two commits earlier) should be split and/or squashed.

Real functional bugs found: `STQ_Um` declares its stored register as an *output*
operand (bd89c4b); the GNU-macro expansions in `matchAndEmitInstruction` bypass
subtarget predicates so `ldbu`/`ldwu` assemble on non-BWX (8b354e4);
`getBranchTargetEncoding` blindly casts any `!specifier` to a fixup kind
(8b354e4); `isEligibleForTailCallOptimization` never looks at the *caller's*
calling convention though the comment says it does (3fec1c4); `call_pal` has no
range check on its 26-bit field (5559223); a relaxed branch can set `UsesGP`
after the prologue is emitted, which now also mislabels `st_other`
(e3a4cf0 + e3edbce).

Bulleted commit-message bodies (522419f, bd89c4b) are not LLVM style.

---

## 91d8b67e3eda — [Alpha] Add a target transform info cost model

* `AlphaTargetTransformInfo.h:106` — `getArithmeticInstrCost` multiplies by 4
  (MUL) / 64 (div/rem) for **every** `CostKind`, including `TCK_CodeSize`.  A
  `sdiv` is one `jsr` plus argument setup: costing it 128 for code size will
  distort inlining and the size-based heuristics.  Guard on
  `CostKind == TTI::TCK_RecipThroughput` (or return `BaseT` cost for
  `TCK_CodeSize`), as RISCVTTIImpl does.
* `AlphaTargetTransformInfo.h:106-110` — the call drops `Args` and `CxtI` when
  forwarding to `BaseT::getArithmeticInstrCost`.  Pass them through.
* `AlphaTargetTransformInfo.h:69` — `if (BitSize > 64) return TTI::TCC_Free;`
  is silently claiming an i128 constant is free.  Other targets return
  `TTI::TCC_Free` only for `BitSize == 0` and fall through otherwise; at
  minimum this deserves the *why*, not a bare early return.
* Tests: only `getArithmeticInstrCost` is covered.  `getIntImmCost` (the
  lda/ldah pricing the commit message advertises), `getPopcntSupport` (CIX) and
  `shouldBuildLookupTables` have **no test at all**.  Add a
  `CostModel/Alpha/int-imm.ll` and a `-mcpu=ev67` popcount case; the CIX path is
  a subtarget-conditional and is exactly the kind of thing that silently rots.
* `llvm/test/Analysis/CostModel/Alpha/arith.ll:4-5` — the block comment
  ("The multiplier is slow and there is no divide instruction, so ...") restates
  the commit message; the per-function comments below already say it.
* File header comment lines 9-13 restate the class's contents ("It supplies
  Alpha-specific answers to a few cost queries (the slow multiplier, ...)").
  LLVM file headers are one line; drop the inventory.

## 8d8e56a1c8fc — [Alpha] Test inline memcpy/memset expansion and min/max lowering

* Test-only commit whose message ("Neither depends on the cost model") is a
  reply to a review comment, not a description of the change.  Upstream this
  reads as a non sequitur.  Either fold these tests into whichever commit made
  the lowering work (they test pre-existing generic lowering, so a plain
  "[Alpha] Add tests for inline memcpy/memset and min/max lowering" is the right
  message), or drop the first clause.
* `memops.ll:12` — `CHECK-NOT: memcpy` / `CHECK: ldq` / `CHECK: stq` /
  `CHECK-NOT: memcpy` around a 32-byte copy asserts almost nothing: a single
  `ldq`+`stq` pair satisfies it.  Check the four pairs, or use
  `--implicit-check-not=jsr`.
* `memops.ll` / `minmax.ll` — generic file names.  `memops.ll` in particular is
  a grab-bag name; `memcpy-inline.ll` matches the convention used by other
  targets.
* `minmax.ll:38` — the `absv` CHECK-NEXT chain (`sra`/`xor`/`subq` in fixed
  registers) is brittle against any scheduling change; prefer FileCheck
  variables for the temporaries as the other functions in the file do not.

## 3fec1c41ad39 — [Alpha] Support sibling and tail calls

**Structure — split this.**  It is three changes: (a) tail-call lowering,
(b) removing `Uses = [R26]` from `RET` plus the `addLiveIn(R26)` in
`LowerFormalArguments`, (c) not attaching a register mask to the tail call.
Paragraphs 3 and 4 of the message are written as *fixes to earlier commits in
this same series* ("RET listed $26 as an explicit use ... so
`-verify-machineinstrs` failed", "A tail call was also given the normal
call-preserved mask").  (b) is an independent bug fix for the commit that
introduced `RET` and should be squashed backwards, taking `verify-return.ll`
and the `branch-three-terminators.mir` liveness edits with it; (c) belongs in
(a) as part of the design, not as a retrospective.

* `AlphaISelLowering.cpp:737` — the comment says the callee "must use the same
  (C or fast) convention as this function", and the header comment at
  `AlphaISelLowering.h:249` says "shares the caller's C calling convention", but
  the code only inspects `CalleeCC`.  `MF.getFunction().getCallingConv()` is
  never read.  Either compare the two or fix both comments.
* `AlphaISelLowering.cpp:734` — no check on the *caller* being variadic, and no
  check for `MF.getFunction().hasStructRetAttr()` mismatch (the classic
  sret-tail-call miscompile).  Compare with `RISCVTargetLowering::
  isEligibleForTailCallOptimization`, which is the model cited elsewhere in the
  series.
* `AlphaInstrInfo.td:1570` — `TCRETURN` has an empty `(ins)` list and no
  `variable_ops`, yet `AlphaTCReturn` is `SDNPVariadic` and `LowerCall` pushes
  `DAG.getRegister(...)` operands onto it.  This appears to work via
  InstrEmitter's glue handling, but every other target spells the argument
  registers with `variable_ops` (see RISCV `PseudoTAIL`).  Please confirm the
  argument registers really do end up as implicit uses in the MIR (a
  `-stop-after=finalize-isel` MIR test would nail it down).
* Tests: only the `byval` rejection path is covered.  Neither
  `NumStackBytes != 0` (a 7+ argument callee) nor `IsVarArg` nor the
  calling-convention rejection has a test, and those are the conditions whose
  failure miscompiles.  Add them.
* `tailcall.ll:8-10` — the three-line comment before `tail_direct` restates the
  commit message paragraph verbatim.  One line is enough.
* `verify-return.ll:4-7` — the header comment explains the RET modelling; that
  explanation belongs (once) next to the `let isReturn` line in the .td, which
  already has it.

## d4fce8bec379 — [Alpha] Signed division by a constant via magic multiply

* Identity `mulhs(a,b) = umulh(a,b) - (a<0 ? b : 0) - (b<0 ? a : 0)` is correct.
* `LowerMULHS` hardcodes `EVT VT = MVT::i64` and ignores `Op.getValueType()`.
  It happens to be right (only i64 is Custom), but an assert
  (`assert(Op.getValueType() == MVT::i64)`) costs nothing and documents it.
* `sdiv-const.ll:8-10` — `CHECK-NOT: __divq` / `CHECK: umulh` is fine, but the
  test never checks that the *result* is right-shaped (no check of the `sra`
  correction or the final add).  Consider `llc ... | FileCheck` with the full
  sequence for `sdiv10`, since a wrong magic constant would still emit `umulh`
  and pass.
* No test for a **negative** constant divisor (`sdiv i64 %x, -7`), which
  exercises the second sign-correction term the commit message says folds away
  for positive divisors.

## 3cb33f89d6f7 — [Alpha] Factor a constant multiply into scaled-add chains

* `mulSeqCost` returns the sentinel `100` for "needs a multiply".  Magic number;
  use `UINT_MAX` or a named `constexpr unsigned NoChain`.
* `PerformDAGCombine` handles only `ISD::MUL` yet is the target's general
  combine entry point.  Fine, but the function-level comment (lines 199-204)
  documents the *multiply factoring*, not the hook.  When a second combine is
  added this comment becomes a lie.  Move it onto a `performMULCombine` helper.
* Negative constants are never factored: `C->getZExtValue()` for `mul x, -25`
  gives a huge `V`, `mulSeqCost` returns 100, and the combine bails.  Not a
  bug, but the message claims "peels a factor of 3, 5 or 9 off the constant's
  odd part" without that caveat.
* `V / F >= 3` in the peel loop has no explanation while the identical-looking
  guard in `mulSeqCost` has none either; a one-line *why* (avoid emitting
  `mul x, 1`/`mul x, 2` which the shift path already handles) would help.
* `mul-const.ll:81-88` — `mul100` checks only `CHECK-NOT: mulq` + `CHECK: ret`.
  That passes for *any* non-multiply expansion, including a 12-instruction one,
  which is precisely what the cost cutoff is supposed to prevent.  Check the
  actual `s4addq`/`sll` chain as `mul25` does.
* Good: `mul10000` covers the cost-cutoff rejection.

## 3e1b8960e0e7 — [Alpha] Rematerialize constants instead of spilling them

* Correct approach (`isConstant` on `$31`/`$f31` is what makes
  `isReallyTriviallyReMaterializable` reject the `$30`/`$15`-based lda).
* `remat-const.ll` has **no negative check**.  Three `lda $16, 1234($31)` lines
  would also appear if the value were simply re-materialized by ISel per call.
  Add `; CHECK-NOT: stq $9` (or `--implicit-check-not=stq $9,`) so the test
  fails if the spill comes back — that is the behaviour the commit exists for.
* `AlphaRegisterInfo.td:63,73` — the two `// $31 is hardwired to zero, so it
  holds a constant value.` comments restate `isConstant = 1`.  One of them, at
  most, and it should say *why it matters* (rematerialization), which is
  information the reader cannot get from the code.
* Splitting `foreach i = 0-31` into `0-30` plus a hand-written `F31` to attach
  `isConstant` is more churn than needed; `let isConstant = 1 in def F31` inside
  a second `foreach 31-31` reads worse, but a `defvar`/`!eq(i,31)` on
  `isConstant` inside the existing loop would keep it one construct.

## acfc9cad60bb — [Alpha] Model truncation as free and sign-extension as cheaper

**Must be squashed with 6694f35fe1e1 (below).**  As it stands this commit
introduces a wrong `isSExtCheaperThanZExt` and a test that, by the next
commit's own admission, cannot detect the bug.

* `AlphaISelLowering.h:240` — `isSExtCheaperThanZExt` returning true for every
  integer pair is wrong for i1/i8/i16 (fixed 8 commits later).
* `ext-free.ll` (as added here) does not reach the hook at all.

## 6694f35fe1e1 — [Alpha] Prefer a sign extension only from i32

* **Squash into acfc9cad60bb.**  The message's third paragraph ("ext-free.ll
  could not see any of this: neither of its functions reached the hook, and both
  passed with the change reverted") is a review-cycle artefact.
* **Remove the tracker IDs**: `Fixes ALPHA-021, and the ext-free.ll half of
  ALPHA-T02.` — meaningless outside this workflow, and not an upstream
  convention.
* `AlphaISelLowering.h:237-244` — the **old comment was left in place** above
  the new one, and the two now contradict each other:

  ```
  // A value narrower than a register is held sign-extended (addl/ldl and the
  // like sign-extend), so a sign extension is a single instruction while a zero
  // extension needs an extra zapnot; prefer the sign extension.
  // Only i32 -> i64.  A 32-bit value is held sign-extended, so widening one is
  // free ...  Every narrower width goes the other way ...
  ```

  Delete the first three lines.  Eight lines of comment on a one-line predicate
  is also more than the fact warrants; two lines suffice after the squash.
* `ext-free.ll:29-34` — the comment "This function is the case that regressed
  when isSExtCheaperThanZExt answered yes for every integer pair -- it emitted
  `sll 56' / `sra 56' here on ev4" narrates the series' own history.  After the
  squash it should read as a statement about the target, not about a bug that
  never shipped.
* Good: adding the `-mcpu=ev4` RUN line is the right call, since BWX is what
  changes the cost.

## 3f92812b20ab — [Alpha] Reassociate operation chains with the machine combiner

* Looks correct; `FmReassoc && FmNsz` for the FP forms matches AArch64/RISCV.
* `reassociate.ll:26-38` — `fchain_strict` relies on exact register assignment
  (`$f0` three times) with `CHECK-NEXT`; a scheduling change will churn it.
  Prefer `--implicit-check-not` plus a check that the second `addt` consumes the
  first's result.
* `AlphaInstrInfo.cpp:266` — `// The inverse (subtract) forms are not modeled
  for reassociation.` describes what `return false` does; the useful comment
  would say *why* (SUBQ/SUBT inverse reassociation is a separate opt-in that
  needs `getInverseOpcode`).
* No test for the `Invert` path or for `AND`/`BIS`/`XOR`; only `MULQ` and
  `ADDT` are exercised.  `ADDQ` — arguably the most common — is untested.

## 542077e8083a — [Alpha] Support the machine outliner

* `AlphaInstrInfo.cpp:321` — `isMBBSafeToOutlineFrom` overrides the base class
  only to call the base class.  Delete the override and the declaration.
* `AlphaTargetMachine.cpp:45` — `this->Options.EnableMachineOutliner = true;`
  overwrites the `TargetOptions` the caller passed in.  `setSupportsDefaultOutlining(true)`
  is the documented target opt-in (`TargetPassConfig.cpp:1244-1249`); AArch64 and
  RISCV set only that.  Forcing `Options.EnableMachineOutliner` from the target
  constructor means Alpha outlines even when the embedder deliberately left the
  option off.  Drop the assignment (and the `this->`, which is a tell) unless
  there is a specific reason, in which case say it in the comment.
* The comment above it ("Enabling the option lets the target pass config add the
  outliner pass; the pass then outlines only where
  shouldOutlineFromFunctionByDefault permits") explains the LLVM framework to
  the reader rather than the Alpha decision.
* `getOutliningTypeImpl` rejects any operand with `getTargetFlags() != 0` and
  any of seven physregs.  Reasonable, but it also rejects `MO.isMBB()` twice
  over (already covered by `isBranch`/`isTerminator`).  Minor.
* `machine-outliner.ll` — the `f1`/`f2`/`f3` CHECK-LABELs are interleaved with
  the `bsr` checks *before* the function bodies, so the file reads oddly; more
  importantly there is no check that the outlined function's body matches the
  five instructions, and no test of the `$23`-live-across-the-call rejection in
  `getOutliningCandidateInfo`, which is the one Alpha-specific safety condition
  in the commit.  Add a case where `$23` is live (e.g. via an inline-asm clobber
  or a `{$23}` register variable) and check no `bsr` is emitted.
* Generic function names `f1`/`f2`/`f3`/`big` — the series elsewhere names test
  functions after what they test.

## e3a4cf0a6d35 — [Alpha] Relax branches to out-of-range targets

* Range is right: 21-bit signed displacement in instruction units → `isInt<23>`
  on the byte offset.  Good.
* `AlphaFrameLowering.cpp:98-104` — `3u << 20` is an unexplained 3 MiB margin
  against a 4 MiB range.  Name it (`static constexpr uint64_t GPRequestThreshold`)
  and say why the margin is 1 MiB.  More seriously, the size estimate runs at
  `determineCalleeSaves` (before `ExpandPostRAPseudos`), so `RMW_STOREI8`/
  `RMW_STOREI16` — which expand to multiple instructions in
  `expandPostRAPseudo` — are each counted as 4 bytes.  A function full of
  pre-BWX byte stores can therefore be under-estimated.  Either count expanded
  pseudos or note the assumption.
* `AlphaInstrInfo.cpp:305` — `insertIndirectBranch` calls `setUsesGP()` after
  the prologue has already been emitted, so it cannot cause an `ldgp` to appear.
  It is not dead, though: after e3edbce2ea45 it *does* change the emitted
  `st_other` to `STO_ALPHA_STD_GPLOAD` for a function whose prologue has no
  `ldgp`.  That combination is a linker-visible lie.  Either drop the call and
  rely solely on the frame-lowering estimate, or make the estimate authoritative
  and assert here.
* The `RestoreBB` and `RS` parameters are ignored without comment; a one-liner
  ("$28 is reserved, so nothing needs scavenging and no restore block is
  required") would head off the reviewer question.
* `branch-relaxation.ll` — good, `.space 5000000` is honoured by
  `TargetInstrInfo::getInlineAsmLength`, so the test really does force
  relaxation.  Consider also testing the *backward* far branch and a function
  just under the threshold (the `3u << 20` boundary is untested).

## 0ad758bec362 — [Alpha] Fold a small constant into a comparison

* `CMPLEi`/`CMPULEi` are defined and pattern-matched but almost certainly
  unreachable from ISel: `x <= C` is canonicalized to `x < C+1` before
  selection, exactly as the comment acknowledges for the GE/UGT cases.  They are
  also **untested** — `setcc-imm.ll` covers only EQ, NE, SLT, ULT and SGT-vs-0.
  Either add tests that reach them (they are still needed by the assembler for
  `cmple $r, 5, $r`) or say in the comment that they exist for the assembler.
* `x > C` for a nonzero small constant still materializes `C` into a register
  (`Pat<(setcc GPRC:$a, GPRC:$b, SETGT), (CMPLT $b, $a)>`) because the immediate
  cannot sit in the first operand.  The comment at
  `AlphaInstrInfo.td:1115-1118` addresses only the zero case.  Worth a note that
  the general `>` against a literal is left as two instructions.
* `AlphaInstrInfo.td:439-440` — the class comment "(so a comparison against a
  small constant, in particular zero, needs no separate materialize)" repeats the
  commit message; the `Pat` block below repeats it a third time.
* No MC test that the new `cmpXXi` forms round-trip through the assembler and
  disassembler, even though they add five new encodings.

## c864fb8e4ad9 — [clang][Alpha] Accept the GCC inline-asm constraint letters

* **Commit message is wrong**: "the fixed-register letters a/b/c/v" are *not*
  added.  The diff adds I/J/K/P/S/L/M/N/O/G/H/T/Q/U/R only.  Either add them or
  fix the message.
* **Inconsistent with the LLVM half (a0713494cac5)**: clang accepts `G` and `H`
  (FP constants) but `AlphaTargetLowering::getConstraintType` does not handle
  them, so an asm using `G` gets past the front end and then fails in ISel.
  Likewise `R`: clang marks it neither register nor memory, LLVM classifies it
  `C_Memory`.  These two commits disagree on the model and are separated by
  three unrelated commits — put them adjacent, and preferably merge the
  constraint table decisions into one reviewable change.
* `Alpha.h:171` — `case 'R': // A symbolic operand within one instruction of the
  referent.` GCC's alpha `R` is `direct_call_operand`, i.e. a call target
  symbol.  The comment describes something else.
* **No test at all.**  A clang `validateAsmConstraint` change with 15 new letters
  and zero `clang/test/CodeGen` or `clang/test/Sema` coverage.  At minimum a
  `-fsyntax-only -verify` test that a bad `I` value is diagnosed and a good one
  is not.

## 156e4bdfe5e8 — [clang][Alpha] Derive the extension set and macros from -mcpu

* **First message paragraph describes pre-existing code.**  `initFeatureMap`
  with the CPU→extension mapping is already in the parent commit
  (`Alpha.h:73-96` at `156e4bdfe5e8^`), comment and all — the message even
  paraphrases that comment.  What this commit actually does is: add the family
  and extension *macros*, and add `ev45`/`pca56` to `isValidCPUName`.  Rewrite
  the message accordingly.
* `isValidCPUName` gains `ev45` and `pca56`, but `init-alpha-cpu.c` tests
  neither `ev45` nor `generic` — and `ev45` is the one whose family macro
  (`__alpha_ev4__`, via the fall-through default) is easiest to get wrong.
* `Alpha.cpp:62-68` — the family selection is a chain of string compares
  duplicating the CPU list in `isValidCPUName` and again in `initFeatureMap`.
  Three places to update when a CPU is added.  A single table would be better.
* `__alpha_max__` for MVI: correct (GCC spells the MAX extension `max`), but the
  feature is named `mvi` internally — worth one comment saying they are the same
  thing, since the mismatch will otherwise look like a bug.

## 522419f3c6b0 — [Alpha] Reserve registers for -mno-fp-regs and -ffixed-$<n>

* Bulleted commit-message body is not LLVM style; write it as prose.
* **`-mno-fp-regs` is not GCC-compatible as implemented.**  GCC's `-mno-fp-regs`
  implies `-msoft-float`: FP values are passed in integer registers and results
  in `$0`.  Here the FP registers are merely reserved, so any FP code makes the
  register allocator abort — and `no-fp-regs.ll:20` **asserts the abort as the
  expected behaviour** (`; NOFP: no registers from class available to allocate`).
  A test whose expected output is a compiler fatal error is a red flag; and the
  commit message's kernel justification ("so it need not save and restore
  floating-point state") does not cover what happens when FP does appear.  At
  minimum, diagnose it as a proper `report_fatal_error`/diagnostic with an
  Alpha-specific message, and say in the message that soft-float is not
  implemented.
* No test for `-ffixed-$<n>` on the LLVM side at all — `reserve-rN` is 32
  features and `getReservedRegs`'s loop over `GPRCRegClass` is untested.  Add a
  `llc -mattr=+reserve-r9` test showing `$9` is not allocated.  (The clang side
  in 14fa704d only tests the driver's `-target-feature` string, not the effect.)
* Reserving a register that is also the frame or global pointer, or reserving
  `$16`-`$21` while they carry arguments, is silently accepted.  SPARC/RISCV
  warn.  Worth a note or a diagnostic.

## 14fa704d3919 — [clang][Alpha] Add -mno-fp-regs and -ffixed-$<n>

* `Options.td:7225-7231` — the 64 new options (`-ffixed-0`..`-ffixed-31` and the
  `$` aliases) are defined unconditionally for **all** targets.  `-ffixed-8` will
  now be accepted by the driver for x86 and silently do nothing (only an
  unused-argument warning).  Other targets namespace these (`-ffixed-x9` on
  AArch64, `-ffixed-g1` on SPARC).  Expect upstream pushback; consider
  `-ffixed-$N` only, or gate the group.
* The `ALPHA_FIXED(N)` macro + 32-entry array in `Arch/Alpha.cpp:60-72` is
  unavoidable given the option-ID design, but a comment saying *why* it cannot
  be a loop (option IDs are not contiguous by construction) would save the next
  reader the question.
* Tests are driver-only (`-###`).  Fine for this commit, given 522419f owns the
  behaviour — but 522419f has no `-ffixed` test either, so the feature is
  end-to-end untested.  One of the two commits must carry it.

## a0713494cac5 — [Alpha] Accept the GCC inline-asm constraint letters

* `AlphaISelLowering.cpp:322-323` — `case 'M': case 'T': Ok = true;` accepts
  **any** constant, directly contradicting the commit message ("Each letter's
  range is checked when the operand is lowered, so a value that does not fit is
  rejected there").  `M` is the zap byte-mask constraint and has a well-defined
  set; if the check is genuinely not worth implementing, say so in the comment
  instead of claiming the opposite in the message.
* Missing `G`/`H` handling (see c864fb8 above) — clang lets them through, LLVM
  does not know them.
* `getConstraintType` returns `C_Memory` for `R`, which GCC defines as a direct
  call target; treating it as a memory operand will produce a `disp(base)`
  rendering for what should be a bare symbol.  Untested either way.
* Tests: `inline-asm-constraints.ll` covers `I`, `K` and `rJ` only.  `L`, `M`,
  `N`, `O`, `P`, `S`, `T`, `Q`, `R`, `U` — ten of the thirteen letters — have no
  test, including all three memory/symbolic ones.  Add at least one negative
  test (a value outside the range falling back to `r`), which is the behaviour
  the message singles out.
* `AlphaISelLowering.h:135` — the existing comment `// Inline assembly: "r"
  selects an integer register, "f" a floating-point one.` is now badly out of
  date above a function that handles thirteen more letters.

## 5f3b5032c9ed — [Alpha] Name a physical register in inline asm and a register variable

* `AlphaISelLowering.cpp:372` — the integer branch returns `GPRCRegClass`
  regardless of `VT`.  `asm("":"={$0}"(double_value))` will bind an f64 to an
  integer register and trip the verifier.  Check `VT` (or reject non-integer
  VTs) as the FP branch does.
* **Duplicate test.**  The `physreg` function added to
  `inline-asm-constraints.ll:38-43` is byte-for-byte the same function, with the
  same CHECK lines, as `inline-asm-physreg.ll:8-12` added in the same commit.
  Delete one.
* `getRegisterByName` uses `report_fatal_error` for an unknown name.  Consistent
  with AArch64, but note the kernel spells some register variables with ABI
  names (`$8` here, but `$fp`/`$gp` elsewhere); accepting only the numeric form
  means `register void *sp __asm__("$sp")` hard-fails the compiler rather than
  producing a diagnostic.  Consider routing through the same alias table the
  asm parser needs (see bd89c4b).
* No test for `llvm.write_register` — only the read side.

## 4d135b7ab1d1 — [Alpha] Lower a block address

* **The test does not test the change.**  `blockaddress.ll` has one function,
  `cg`, that takes the target as a *parameter* and does `indirectbr ptr %p`.
  Nothing in it produces an `ISD::BlockAddress` node, so `LowerBlockAddress`,
  the two new `tblockaddress` patterns and the new `PrintAsmOperand` case are
  never exercised.  The two CHECKs (`jmp $31, ($16), 0` and `.quad .Ltmp0`) both
  pass without the patch — the `.quad` comes from generic global-initializer
  emission.  Rewrite so the block address is materialized in a register:

  ```llvm
  define void @cg() {
    br label %dispatch
  dispatch:
    indirectbr ptr blockaddress(@cg, %target), [label %target]
  target:
    ret void
  }
  ```

  and check `ldah $\{\{.*\}\}, .Ltmp0($29)` + `!gprelhigh` / `lda` + `!gprellow`,
  which is what the commit message promises.
* `AlphaAsmPrinter.cpp:106-113` — the new `MO_BlockAddress` case in
  `lowerOperand` handles `MO.getOffset()`, but the `MO_JumpTableIndex` case
  immediately above does not; if offsets matter here they matter there.
  Inconsistent, and untested.
* Trailing blank line at end of `blockaddress.ll`.

## d3c74d373b76 — [Alpha] Lower the return and frame address builtins

* `LowerFRAMEADDR` sets `setFrameAddressIsTaken(true)` only on the level-0 path,
  while `LowerRETURNADDR` sets `setReturnAddressIsTaken(true)` before its level
  check.  The asymmetry is deliberate (and commented) but the two hooks now
  behave differently for level != 0 for no stated reason; make them consistent
  or explain both in one place.
* `builtin-address.ll:26-28` — `; CHECK-NOT: $15` between the label and
  `lda $0, 0($31)` is a broad negative that will also fire on an unrelated
  `$15` save; scope it or use `--implicit-check-not`.
* No test of `__builtin_return_address(1)` (the nonzero-level RA path), though
  the message calls it out.
* Otherwise correct: `hasFPImpl` gaining `isFrameAddressTaken()` is required and
  the test checks the prologue ordering.

## 816220a5e5c3 — [Alpha] Expand a single-bit sign extend

* **Code comment contradicts the commit message and the test.**
  `AlphaISelLowering.cpp:129`: `// No single-bit sign-extend instruction; expand
  it to an sll/sra pair.` — but the message says (and `sext-inreg-i1.ll` checks)
  `and`/`subq`.  Fix the comment.
* Otherwise minimal and correct.  Consider adding an `-mcpu=ev4` RUN line for
  symmetry with the neighbouring i8/i16 `SIGN_EXTEND_INREG` handling, which is
  BWX-conditional two lines above.

## 55592232601b — [Alpha] Assemble PALcode calls

* `palfn` is a bare `Operand<i64>` with **no range check**.  `call_pal
  0x4000000` (27 bits) will silently truncate into `Inst{25-0}`; GNU as
  diagnoses it.  Give it an `AsmOperandClass` with a `isUInt<26>` predicate.
  The test's "maximum" case (`call_pal 0x3fffff`) is only 22 bits, so the
  boundary is untested in both directions.
* `PalCallForm` (AlphaInstrFormats.td:151) is `PalForm` with the function code
  as an operand instead of a template parameter; the two are otherwise
  identical.  Consider parameterizing rather than duplicating.
* `callsys` and `chmk` map to the same code 0x83; only one can be the preferred
  disassembly, and neither is (both print `call_pal 131`).  That matches the
  message, but the disassembler test only covers 0x81 and 0x9e — none of the
  named aliases round-trip in `alpha.txt`.
* The claim "The encodings are byte-exact with the cross-binutils assembler and
  round-trip through the disassembler" belongs in the review discussion, not the
  permanent commit message; if it stays, it should say *which* binutils.

## bd89c4b75bce — [Alpha] Assemble the kernel's hand-written assembly

**Structure: split.**  Seven independent bullet groups in one commit (memory
operand forms; literal operand forms; misc-format barriers/hints; branch/return
spellings; register aliases; `.word` aliasing; ECOFF directives).  Each is
separately reviewable and separately testable, and the ECOFF-directive part is
rewritten two commits later by 8b354e4310d1 — that part should either be
dropped here or the two commits squashed.

* **Bug — `STQ_Um` declares an output operand:**

  ```
  def STQ_Um : MForm<0x0f, (outs GPRC:$Ra), (ins memri:$addr), "stq_u $Ra, $addr", []>;
  ```

  A store defines nothing.  Compare `LDQ_Um` directly above, which correctly
  has `(outs GPRC:$Ra)`.  `STQ_Um` should be `(outs), (ins GPRC:$Ra,
  memri:$addr)`.  As written `MCInstrDesc::getNumDefs()` is 1 for a store, which
  will mislead any MC-layer consumer (llvm-mca, the disassembler's operand
  handling, future MC analyses).
* **The `matchRegister` StringSwitch duplicates the alt-names already in
  `AlphaRegisterInfo.td`** (`def R27 : GPR<27, "$27", ["$pv", "$t12"]>` etc.).
  Use the generated `MatchRegisterAltName` instead of a hand-maintained
  30-entry table that will drift from the .td.
* **Message claims "The $rN register spellings"** — there is no `$r16` handling
  anywhere in the diff (or in the tree: no `MatchRegisterAltName`, no `"$r"` in
  the .td).  `$rN` is the *MIR* spelling, unrelated to the asm parser.  Remove
  the claim.
* `RETb` sets `hint = 1` and `RETab` sets `hint = 0`.  GNU as encodes `ret` with
  hint 1 in both spellings.  If that is deliberate, say why; if not, this is a
  byte-level divergence from GNU as, which the commit's "all byte-exact against
  GNU as" claim does not survive.  `kernel-asm.s` only covers `ret ($26)`.
* `ldiq` is aliased to the 16-bit `LDAi` only; `ldiq $0, 0x12345` — which GNU as
  assembles as a macro — will be a hard error.  The comment admits it; the
  commit message's "the kernel's hand-written assembly" framing does not.
* `P.addAliasForDirective(".word", ".2byte")` is unrelated to every bullet in
  the message and is tested two commits later (42b87b7).  Move it, with its
  test, into the directives commit.
* Test coverage gaps in `kernel-asm.s`: `ldiq`, `andnot` literal form, `BRr`,
  `BRt`, `RETab`, `JMPab`, `LDAHi`, and 26 of the 30 `BLit` definitions are
  untested; the ECOFF directives are tested only in 42b87b7.  With this many new
  encodings, an encoding line per instruction class is the minimum.
* Encodings I spot-checked are right: `wmb` 0x4400, `trapb` 0x0000, `excb`
  0x0400, `fetch` 0x8000, `fetch_m` 0xa000, `ecb` 0xe800, `wh64` 0xf800,
  `wh64en` 0xfc00, `unop` = `ldq_u $31, 0($30)` = 0x2ffe0000.

## 42b87b72190d — [Alpha] Make .align count a power of two, as GNU as does

* The `AlignmentIsInBytes = false` change itself is correct and important.
* **The test barely tests it.**  `directives.s` puts `.align 3` at offset 8,
  which is already 8-byte aligned, so the padding is zero either way; the test
  only fails without the patch because `.align 3` is *rejected* as a non-power-of-2
  in byte mode.  Put the `.align 3` at offset 4 (or 12) and check that 4 bytes of
  padding appear — that is the semantic difference the commit is about.
* **Test is in the wrong commit.**  Two thirds of `directives.s` exercises the
  ECOFF directives from bd89c4b75bce and the `.word` → `.2byte` alias also from
  bd89c4b75bce.  The commit message even says so ("it gets its own test
  alongside the procedure-descriptor directives").  Move those checks back to
  the commit that adds the behaviour and keep an `align.s` here.
* Message: "every hand-written `.align 3' was aligning to three bytes -- rounded
  up to four -- instead of eight" — as noted, byte mode *errors* on 3 rather
  than rounding.  Fix the description.

## ab9cbc94f3c2 — [Alpha] Pad code alignment with unop

* Correct: `unop` = `ldq_u $31, 0($30)` = 0x2ffe0000, matching GNU as.
* `writeNopData` still does `OS.write_zeros(Count % 4)` for a sub-word tail;
  pre-existing, but now that the filler is being made GNU-as-compatible it is
  worth a word on why the remainder is zeros rather than `.byte 0x00` padding
  the way gas does.
* The test is good (it distinguishes `nop` from the filler).  Consider also
  checking `-filetype=obj` alignment padding in a `.text` subsection.

## cb813e89fd28 — [Alpha] Accept a $-prefixed local label in an operand

* Behaviour is right and both new syntaxes are tested.
* `parseOperand` now falls through to `parseExpression` when `tryParseRegister`
  fails on a `$` token.  Does `tryParseRegister` guarantee it consumes nothing
  on failure?  If it can consume the `$` before failing, the expression parser
  sees a truncated identifier.  Worth an explicit `getLexer().getTok()`
  save/restore, or a comment stating the guarantee.
* `dollar-label.s:26` — `# CHECK: lda $31, {{-?[0-9]+}}($16)` accepts any
  displacement, including a wrong one.  The `$exception-99b` difference is a
  known constant (4); check it, and add a `CHECK-NOT: R_ALPHA` so the test
  proves the difference folded rather than emitting a relocation.
* `br $target` in the test hits the `BRt` form added in bd89c4b; fine, but that
  makes this test depend on the previous commit's asm-parser-only instruction —
  worth being aware of when reordering.

## 8b354e4310d1 — [Alpha] Assemble the GNU as macros and procedure directives

**Structure**: this rewrites the `.ent`/`.end`/`.prologue` handling added by
bd89c4b75bce two commits earlier (from "accept and ignore" to "set st_other").
Squash the directive halves together, or add them here only.

* **Bug — subtarget predicates are bypassed.**  The `ldq/ldl/ldbu/ldwu $R,
  symbol` expansion builds `MCInst`s directly and calls `Out.emitInstruction`
  without going through `MatchInstructionImpl`, so the `LDBU`/`LDWU` BWX
  predicate is never checked: `ldbu $0, sym` will assemble happily for
  `-mcpu=ev4`, which has no `ldbu`.  Route through the matcher, or check
  `getSTI().getFeatureBits()` explicitly.
* **Bug — unvalidated specifier→fixup cast.**
  `AlphaMCCodeEmitter.cpp:182-186`:

  ```cpp
  if (auto *SE = dyn_cast<MCSpecifierExpr>(MO.getExpr()))
    Kind = static_cast<Alpha::Fixups>(SE->getSpecifier());
  ```

  Any `!specifier` on a branch target is accepted as that branch's fixup kind,
  so `br target !literal` emits an R_ALPHA_LITERAL against a branch.  Accept
  only `fixup_alpha_brsgp` (and diagnose the rest).
* `ret $Ra, ($Rb), hint` (line ~525) silently discards both `$Ra` and the hint
  and always emits `RETb` (hint = 1).  `ret $26, ($26), 0` therefore assembles
  to something different from what was written.  Given `RETab` exists with
  hint = 0, at least honour the hint.
* The six hand-rolled `if (Mnemonic == ...)` expansion blocks in
  `matchAndEmitInstruction` re-implement operand-shape matching that the
  generated matcher already does.  Upstream will ask for these as
  `processInstruction`-style post-match rewrites or `InstAlias`es where possible.
  In their current form the `Operands.size() == 3 && Operands[1]->isReg() &&
  Operands[2]->isImm() && !isa<MCConstantExpr>(...)` predicate is repeated four
  times verbatim; factor it out.
* `lda $R, symbol` is unconditionally expanded to a `!literal` GOT load.  GNU as
  chooses between the GOT form and a gp-relative `ldah`/`lda` pair depending on
  the symbol's binding and the relaxation model.  The commit claims parity with
  GNU as; please confirm against a local (static) symbol, and test both.
* `STO_ALPHA_NOPV`/`STO_ALPHA_STD_GPLOAD` are defined as local `const unsigned`
  here *and again* in `AlphaAsmPrinter.cpp` (e3edbce2ea45).  They belong in
  `llvm/include/llvm/BinaryFormat/ELF.h` next to the other `STO_*` values (which
  currently has no Alpha entries).
* `gnu-macros.s:56` — `# Bit 3 (0x08) was silently truncated by the old 3-bit
  st_other field.` refers to a defect in some earlier state of the tree, not to
  anything in this commit.  Delete.
* `gnu-macros.s:42` — `bne $1, memcpy !samegp` uses a *conditional* branch for
  the `!samegp` relocation; `br $26, sym !samegp` (the tail-call form gas
  actually emits) is the case that matters.  Test that one.
* The message's second paragraph is four sentences of framework narration
  ("`.end` has to be caught here as well, ahead of the generic directive that
  would stop assembly") that already appears verbatim as a code comment.

## a5df8ad81625 — [clang][Alpha] Forward -mtune to -tune-cpu

* Correct and minimal.
* `alpha-mtune.c:1-2` uses the deprecated `-target alpha-linux-gnu` spelling;
  new tests should use `--target=`.  (The neighbouring `alpha-fixed-regs.c` in
  14fa704d already uses `--target=`, so the series is inconsistent.)
* `-mtune=<garbage>` is forwarded unvalidated; the diagnostic then comes from
  LLVM as an obscure "not a recognized processor" warning.  Other targets
  validate against `isValidCPUName` — cheap to add here since it exists.
* No test that `-mtune` alone leaves `-target-cpu` at the default; the SPLIT
  case only covers `-mcpu` + `-mtune`.

## e3edbce2ea45 — [Alpha] Record each function's procedure kind in st_other

* Correct and well motivated.
* `Sym->setOther((Sym->getOther() & ~STO_ALPHA_STD_GPLOAD) | ...)` — masking with
  `~0x88` to clear both bits is clever but obscure; a named mask
  (`STO_ALPHA_PROC_MASK = 0x88`) makes the intent readable, and the same
  expression appears in the asm parser (8b354e4) so it wants to be shared.
* Interaction bug with e3a4cf0a6d35: `usesGP()` can be set by
  `insertIndirectBranch` *after* the prologue is emitted, in which case this
  function stamps `STO_ALPHA_STD_GPLOAD` on a function that has no `ldgp`.  See
  the e3a4cf0 entry.
* The `.ent`/`.prologue`/`.end` textual emission is untested — the test uses
  `-filetype=obj` only.  Add an asm RUN line; it is also the path an external
  assembler depends on, per the commit message.
* Also untested: a function whose gp use appears only via a call (the common
  case), and the outlined functions from 542077e (which get `NOPV` — worth
  confirming that is intended).

## 0ff321c4e5c5 — [Alpha] Keep the entry ldgp as the first instruction

* The fix (`isSchedulingBoundary` on `LDGP`) is right and the bug description is
  excellent — a genuine miscompile with a clear mechanism.
* **The test cannot fail for that bug.**  `ldgp-first.ll` is:

  ```
  ; CHECK-LABEL: key2index:
  ; CHECK:      ldgp $29, 0($27)
  ; CHECK-NOT:  ldgp
  ```

  `CHECK:` matches the `ldgp` anywhere after the label, so a hoisted `subq`
  ahead of it passes.  The `CHECK-NOT: ldgp` only rules out a second `ldgp`.
  Use `CHECK-NEXT:` (accounting for the `.ent`/`.prologue` lines that
  e3edbce2ea45 now emits between the label and the first instruction), or add
  `; CHECK-NOT: subq` between the label and the `ldgp` check.
* A `-stop-after=post-RA-sched` MIR check would be more robust than matching
  assembly text here.
* Consider testing all three post-RA-scheduling CPUs the message names
  (ev4/ev5/ev56); only ev56 is covered.
