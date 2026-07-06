# Review: commits 161–190 of the Alpha series (`0bf3638..HEAD`)

## Summary

Highest-severity findings, in order:

1. **`e9c48ebf597a` — f128 comparisons treat the OTS return value as a 0/1 boolean.**
   The Alpha OTS comparison routines return **−1 unordered / 0 false / 1 true**
   (gcc `alpha_emit_xfloating_compare` says so verbatim and tests the result with
   `GT`, `LT`, `GE` against zero). This code truncates `$0` to `i1` and negates
   with `XOR 1`, so every comparison involving a NaN gives the wrong answer:
   `fcmp olt` returns *true*, `fcmp une` returns *false*. `SETO`/`SETUO` are not
   handled at all, contradicting the commit message. No test detects any of this.
2. **`da307a314e61` — `Size = 8` on `OTS_CALL`** for a jsr (4) + ldgp expansion
   (8) = 12 bytes. Wrong instruction size feeds branch relaxation. Corrected to
   12 later in the series; it is a live bug across the whole intervening window.
3. **`da307a314e61`/`f94dcafc5813` — the f128 *arithmetic* round argument in `$20`
   is still hardcoded to 2**, while gcc's `alpha_emit_xfloating_arith` passes
   `alpha_compute_xfloating_mode_arg(code, alpha_fprm)`. `f94dcafc5813` is titled
   "Compute the OTS X_floating mode argument the way gcc does" but only fixes the
   two *conversion* sites. `-mfp-rounding-mode=c/m/d` silently does not apply to
   f128 add/sub/mul/div.
4. **`daeff8b04ab4` — the `-msmall-text` bsr path is gated only on
   `isDSOLocal()`**, unlike `isGprelAddressable` two commits earlier, which
   additionally excludes `extern_weak` and `GlobalIFunc`. A `bsr` to a
   `dso_local ifunc` branches to the resolver; a `bsr` to an undefined weak
   symbol has no correct target.
5. **`beb2e35cdf43` adds `clang/test/Driver/alpha-flags.c` with a
   `-mlong-double-128` check one commit before `0443b4690da9` makes that flag
   stop erroring** — a bisect break.

Structural theme: five commits in this chunk are fixes to commits *inside the
same chunk* (`60f44ff8bf5e`→`c09d8db166ac`, `a72d3cda7a3b`→`f76f61f6f042`,
`f5a303bc0d8f`→`515391beac5f`, `f94dcafc5813`→`0d075126ffd7`,
`d1fe4c9bdf44`→`ec861e1e8e2e` for the comment part). All should be squashed
backwards before posting. Five commit messages end with "Fixes ALPHA-0NN",
an internal tracker ID that does not belong upstream.

LLM-tell theme: several commit messages are essays with narrative framing
("The question worth asking is therefore not…"), embedded terminal transcripts,
and — in `d1fe4c9bdf44` — a five-sentence appendix about a *different* bug that
did not reproduce.

---

## `c09d8db166ac` — Address a global gp-relative when the linker resolves it

**Commit message.** By far the longest in the chunk (~40 lines) and structured as
an argument rather than a change description. "The question worth asking is
therefore not how far away the definition is but whether the reference is allowed
to see the definition's own address." — rhetorical framing. The perf/`ld -r`
evidence is genuinely useful and should stay, but trimmed to the essentials; the
paragraph beginning "This is not a saving of one instruction." repeats what the
preceding paragraph already said and is then repeated *again* verbatim as a code
comment in `LowerGlobalAddress`.

**Include ordering.** `AlphaISelLowering.cpp`:
```
+#include "AlphaTargetObjectFile.h"
 #include "AlphaMachineFunctionInfo.h"
```
and
```
+#include "llvm/IR/GlobalIFunc.h"
 #include "llvm/IR/IRBuilder.h"
+#include "llvm/IR/GlobalVariable.h"
```
Both unsorted; clang-format's include sorting would reject this.

**Dead include (surfaces after the next commit).** `AlphaISelLowering.cpp:15`
still includes `AlphaTargetObjectFile.h` at HEAD, but after `60f44ff8bf5e`
removed the `isGlobalInSmallSection` call nothing in that file uses it. Likewise
`AlphaTargetObjectFile::isGlobalInSmallSection` no longer has an external caller
(`grep` shows only `AlphaTargetObjectFile.cpp:60`), so it can become private and
its header doc comment ("…where they are reached by a gp-relative address rather
than a load from the GOT") is now factually wrong — placement no longer implies
addressing.

**Test.** `elf-reloc.ll` gains `-mattr=+small-data` on the RUN line, but the
global it checks is now `dso_local` and would be gp-addressed without the
feature. The flag is inert; it makes the test look like it is testing small-data
when it is not.

`small-data.ll` ends with a run of bare `CHECK:` section/label lines
(`.section .sdata`, `small:`, `.section .sbss`, `smallbss:`, `.section .bss`,
`big:`) with no `CHECK-NEXT`, so nothing pins a symbol to the section directive
immediately above it.

---

## `60f44ff8bf5e` — Do not address a preemptible global GP-relative under -msmall-data

**STRUCTURE: squash into `c09d8db166ac`.** This reverts the `TLOF.isGlobalInSmallSection(...) ||`
arm added two commits earlier, together with test churn that only exists because
the earlier commit's tests were written against the buggy behaviour ("The tests
had to say what they meant. small-data.ll's globals carried no dso_local, so they
were preemptible as written and only reached the gp-relative path through the arm
being removed"). Squashing removes both the bug window and the churn.

`Fixes ALPHA-015.` — drop.

The fix itself is right and the gcc cross-check (`.sbss` placement + `!literal`
load for a default-visibility global at `-fPIC -msmall-data`) is the correct
reference behaviour.

---

## `6ea39c7f99c0` — [clang] Add -msmall-data/-mlarge-data

`Options.td`: the new options open a *second* `let Flags = [TargetSpecific] in {`
block immediately adjacent to the existing one:
```
+}
+let Flags = [TargetSpecific] in {
 def msave_restore : Flag<["-"], "msave-restore">, ...
```
Fold into the surrounding block instead of closing and reopening it.

The forwarding is placed inline in `Clang.cpp::RenderTargetOptions` while every
other Alpha feature in this chunk goes through
`alpha::getAlphaTargetFeatures` in `ToolChains/Arch/Alpha.cpp`. Two mechanisms
for the same job; `-msmall-data` should use `Handle(OPT_msmall_data,
OPT_mlarge_data, "small-data")` like `-msmall-text` does in `058c729e3e1a`.

`Clang.cpp` line exceeds 80 columns:
`    if (Args.hasFlag(options::OPT_msmall_data, options::OPT_mlarge_data, false)) {`

**Test.** `alpha-small-data.c` only checks that the flags are *rejected on x86*.
It never checks that `-msmall-data` produces `+small-data` on Alpha — the actual
functional change. (That check exists, but only in `alpha-flags.c`, added five
commits later in `beb2e35cdf43`.)

---

## `daeff8b04ab4` — Add -msmall-text single-instruction calls

**CORRECTNESS (high).** `AlphaISelLowering.cpp:1966`:
```cpp
BsrCall = Subtarget.hasSmallText() && IsLocal;
```
`IsLocal` is just `G->getGlobal()->isDSOLocal()`. Two commits earlier,
`isGprelAddressable` correctly rejected `hasExternalWeakLinkage()` and
`isa<GlobalIFunc>` for exactly the same reason — the symbol's runtime address is
not the definition's own address. Both cases are equally wrong for `bsr`:

* `dso_local ifunc`: `isDSOLocal()` is true, so a direct call is emitted as
  `bsr $26, ifn !samegp`, which branches to the **resolver**, not the resolved
  implementation. gcc avoids this because `TARGET_BINDS_LOCAL_P` is false for
  ifuncs.
* `extern_weak dso_local` function: the `bsr` has no defined target when the
  symbol is absent.

Factor the checks: `BsrCall = hasSmallText() && isGprelAddressable(*GV)`.

**Untested claim.** The commit message says "A runtime-library callee arrives as
an ExternalSymbolSDNode with no GlobalValue to ask about preemption, so it takes
the GOT as well — memcpy and friends are commonly the shared libc's." No test
covers an `ExternalSymbolSDNode` callee under `-msmall-text` (e.g. a large
`llvm.memcpy`).

**Whitespace.** `ToolChains/Arch/Alpha.cpp` in `058c729e3e1a` leaves a double
blank line after the `small-text` Handle; it is removed again in `f76f61f6f042`.
Fix in place.

**Test.** `small-text.ll` embeds a linker diagnostic as a comment
("`ld: pc-relative relocation against dynamic symbol perror@@GLIBC_2.0`") — fine
as evidence, but the surrounding prose is three paragraphs of tutorial ("The
caller still establishes its own global pointer. It has to: …") repeating the
commit message and the `.td` comment verbatim. Pick one home for the rationale.

---

## `058c729e3e1a` — [clang] Add -msmall-text/-mlarge-text

`alpha-text-model.c` uses the deprecated `-target alpha-linux-gnu` spelling
(also in `f76f61f6f042`, `ff9e7a19f6c7`); other tests in this chunk use
`--target=`. Normalise on `--target=`.

Adjacency to `daeff8b04ab4` is right — keep the split.

---

## `129d23d65d43` — Assemble the ldi/ldiq load-immediate pseudo

**COMMIT MESSAGE IS WRONG.** It ends:
> a full 64-bit value (which GNU as also handles) is diagnosed rather than
> silently truncated.

But `emitLoadImm` explicitly handles the wide case ("Wider: build the high half,
shift it up by 32, then add the low half") and `ldi.s` tests
`ldi $5, 0xdeadbeefcafe`. Nothing is diagnosed. Rewrite the sentence.

**STRUCTURE: SPLIT.** The commit is titled for `ldi`/`ldiq` but also implements
`lda $Rc, sym($Rb)` GOT expansion and `lda $Rc, <wide const>($Rb)`
materialization, with its own test file `lda-materialize.s`. That is a separate
GNU-as-compatibility feature that happens to reuse `emitLoadImm`. Two commits.

**Divergence understated.** The code comment says the `Rc == Rb` case is one
"which under `.set noat` (no scratch) GNU as also cannot handle." GNU as handles
it *with* `$at`, which is the normal case; the divergence is unconditional, not
limited to `.set noat`. Say so plainly.

**Arithmetic review** (no bugs found): `emitConst32`'s `Hi == 0x8000` split into
two `ldah 0x4000` is correct; the `>> 32` high-half compensation in
`emitLoadImm` is correct including `INT64_MIN`; the `ldi $5, 0xdeadbeefcafe`
expectations in the test check out by hand.

**Test gap.** `ldi.s` checks only printed assembly, never encodings; the
error path in `emitLoadImm` (non-`MCConstantExpr` operand → falls through to the
matcher) produces whatever the generic matcher says, which is untested.

---

## `96c00ba31848` — Add the floating-point trap and rounding qualifiers

**Encoding is correct** against gas: `/U` +0x100, `/I` +0x200, `/S` +0x400 →
`su`=0x500, `sui`=0x700, `u`/`v`=0x100, `s`=0x400; rounding in function bits
`<7:6>` (`0xc0 << 5` = instruction bits 11–12) with chopped 0 / minus 0x40 /
normal 0x80 / dynamic 0xc0. The trap-class → suffix table matches gcc's
`get_trap_mode_suffix` for `TRAP_SUFFIX_U_SU_SUI` (class 1), `TRAP_SUFFIX_SU`
(class 2), and `TRAP_SUFFIX_V_SV_SVI` (class 3), including `"v"` for a bare
`-mfp-trap-mode=u`.

**Divergence from gcc, asserted by the test.** Class 4 (integer→float):
```cpp
case 4: // Integer-to-float: inexact only.
  return (IEEE && Inexact) ? "sui" : StringRef();
```
gcc gives `cvtqt` `trap_suffix "sui"`, and `TRAP_SUFFIX_SUI` in
`get_trap_mode_suffix` returns **`"su"` when `alpha_fptm >= ALPHA_FPTM_SU`**.
So under plain `-mieee` gcc emits `cvtqt/su` and this emits bare `cvtqt` — and
`fp-modes.s` pins the divergence with `# SU: cvtqt $f1`. Either match gcc or
document why not; do not silently assert the difference in a test.

**No encoding test.** The commit message says "the encoder puts them in the
instruction word", but `fp-modes.s` is `llvm-mc` text-out only. Add
`-show-encoding` or an objdump run; the function-field bit placement is exactly
the thing that can be wrong.

**Multiple rounding features.** `getFPRoundMode` resolves
`+fpround-chopped,+fpround-minus` by silent priority. Since the driver only ever
sets one, an assert would be better than a silent winner.

---

## `bb3c78006079` — Insert trap barriers for precise arithmetic traps

**Divergence from gcc worth calling out.** gcc does not put a `trapb` after every
trapping FP instruction; `alpha_handle_trap_shadows` inserts one only where the
trap shadow is actually broken (a subsequent write to a register or memory the
handler needs, or a block boundary). This pass emits one unconditionally after
every instruction with a nonzero trap class, including compares (class 2) and
`cvtst` (class 5), which roughly halves FP throughput under `-mtrap-precision=i`.
That may be an acceptable first cut, but the commit message should say it is,
rather than implying parity.

**Pass registration.** `AlphaTrapBarriers` has no `INITIALIZE_PASS` /
`initializeAlphaTrapBarriersPass`, so it cannot be named by `-start-before` /
`-stop-after` and does not appear in `-debug-pass=Structure` by ID. Upstream will
ask for it.

**Test.** `trap-precision.ll` covers `mult`/`addt` only. Nothing covers a
compare (class 2), which the pass *does* follow with a barrier, or the absence of
a barrier after a non-trapping instruction.

---

## `f76f61f6f042` — [clang] Add -mfp-trap-mode, -mfp-rounding-mode, -mtrap-precision

**CORRECTNESS: `-mfp-trap-mode` does not override `-mieee`.** gcc's
`alpha_option_override` seeds `alpha_fptm` from `TARGET_IEEE`/`TARGET_IEEE_WITH_INEXACT`
and then lets an explicit `-mfp-trap-mode=` string overwrite it. Here `-mieee` and
`-mfp-trap-mode=` are independent feature pushes, so:
* `-mieee -mfp-trap-mode=n` still emits `/su` (gcc: no suffix);
* `-mieee -mfp-trap-mode=u` still emits `/su` (gcc: `/u`).
Compute a single trap mode from both options and push exactly one feature.

**STRUCTURE.** `-mieee-conformant` is introduced here with semantics
(`+ieee` + `+trap-precision-insn`, nothing emitted) that `a72d3cda7a3b` then
declares "Both halves are wrong". Move the `-mieee-conformant` option, the
`FeatureIEEEConformant` feature and the `.eflag 48` emission into this commit and
drop `a72d3cda7a3b`'s revert; keep only the `-mtrap-precision=f` rejection and
the new diagnostic as a follow-up if you want it separate.

**Tests.** `alpha-fp-modes.c` covers three of the eight accepted values and
never exercises the `err_drv_unsupported_option_argument` path for any of the
three options. `-mfp-trap-mode=u`, `-mfp-rounding-mode=m`, `-mieee-conformant`
are all untested here (some are covered later in `alpha-flags.c`, i.e. in the
wrong commit).

---

## `a72d3cda7a3b` — Give -mieee-conformant the meaning gcc documents

The gcc reference is right: `alpha.opt` declares `mieee-conformant` as a bare
`Mask(IEEE_CONFORMANT)` with no implications, and `alpha_start_function` emits
`\t.eflag 48\n` under `TARGET_IEEE_CONFORMANT && !flag_inhibit_size_directive`.

**CORRECTNESS: the option is a no-op for object output.** The emission is guarded
by `OutStreamer->hasRawTextSupport()`, so `clang -mieee-conformant -c foo.c` —
the normal compilation mode — emits nothing at all. The test comment asserts
this is fine:
> It is textual only: an object file records the same thing through the
> assembler, so there is nothing to emit when the integrated assembler is
> encoding directly.

That is unverified and, on ELF, likely false — nothing in this series teaches the
integrated assembler what `.eflag` means. There is no `-filetype=obj` RUN line to
check. Either implement the directive in `AlphaAsmParser`/`AlphaTargetStreamer`
so both paths mark the object, or say in the commit message that `-c` is
currently unmarked.

**Diagnostic group.** `warn_drv_alpha_ieee_conformant_needs_modes` is placed in
`InGroup<UnusedCommandLineArgument>`, but the argument is *not* unused — it is
used and the complaint is about a missing companion. Wrong group, and a
target-specific warning is being added to the generic
`DiagnosticDriverKinds.td` block among `warn_drv_unused_x` etc. The message text
also spells options unquoted (`-mtrap-precision=i`) where clang convention is
`'-mtrap-precision=i'`.

**Compatibility.** Turning `-mtrap-precision=f` into a hard error diverges from
gcc, which accepts it. Justified in the message, but flag it for reviewers — it
will break existing build scripts.

`Fixes ALPHA-026.` — drop.

**Test.** `alpha-ieee.c` never exercises `-mieee-conformant -mtrap-precision=i
-mfp-trap-mode=sui`, the other accepted combination that must *not* warn.

---

## `515391beac5f` / `f5a303bc0d8f` — precise-trap processors, then un-marking generic

**STRUCTURE: squash `f5a303bc0d8f` into `515391beac5f`.** The former adds
`FeaturePreciseArithTraps` to `generic`; the latter removes it two commits later
with "the generic processor carried FeaturePreciseArithTraps, so
-mtrap-precision=i and -mieee-conformant emitted no trapb at all unless an -mcpu
was given." That is a bug introduced and fixed inside the same chunk, and the
test churn ("trap-precision.ll had to pass -mcpu=ev5 on both of its original RUN
lines to see any barrier") exists only because of it.

The final state is correct and matches gcc's `alpha_option_override`, which
forces `alpha_tp = ALPHA_TP_PROG` for `PROCESSOR_EV6`.

`Fixes ALPHA-005.` — drop.

---

## `807c84d1e95a` — Add -msafe-partial atomic misaligned stores

**CORRECTNESS: the memory operand is dropped.** As committed:
```cpp
return DAG.getNode(AlphaISD::SAFE_USTORE, dl, MVT::Other,
                   {Chain, Val, Ptr, DAG.getConstant(Bytes, dl, MVT::i64)});
```
built with `getNode`, not `getMemIntrinsicNode`, and the SDNode is declared
without `SDNPMemOperand`. Volatility, alias info and the `MachinePointerInfo` are
all lost — a `volatile` misaligned store under `-msafe-partial` stops being
volatile. Note the code being *replaced* explicitly computed
`MachineMemOperand::MOVolatile`. This is repaired in `da307a314e61` (which adds
`SDNPMemOperand` and switches to `getMemIntrinsicNode`); squash the fix back.

**Redundant second loop.** `emitPartialStore` always emits both `LoopLo` and
`LoopHi`. When the field lies inside one quadword the high loop degenerates to
`Old | 0` stored back — functionally a no-op, as the comment says, but it is
still a full `ldq_l`/`stq_c` retry loop on every misaligned store, doubling the
cost of the common case. If gcc's
`alpha_expand_unaligned_store_safe_partial` behaves the same, say so; otherwise
skip it when `(Addr & 7) + Bytes <= 8` is provable.

**Test.** `safe-partial.ll` covers a single case (`store i32, align 1`).
No coverage of `i16`, `i64`, or of an *aligned* store under `+safe-partial`
staying an ordinary `stl` — the last is the one that would catch an over-broad
`hasSafePartial()` guard in `LowerSTORE`.

---

## `beb2e35cdf43` — [clang] Add -msafe-partial

**BISECT BREAK.** This commit creates `clang/test/Driver/alpha-flags.c`
containing:
```
// RUN: %clang --target=alpha-unknown-linux-gnu -mlong-double-128 \
// RUN:   -### -c %s 2>&1 | FileCheck %s --check-prefix=LONGDOUBLE
// LONGDOUBLE-NOT: error:
// LONGDOUBLE-NOT: warning:
```
`-mlong-double-128` is only accepted on Alpha as of `0443b4690da9`, the *next*
commit. At this commit `Clang.cpp` falls into
`err_drv_unsupported_opt_for_target`, so the test fails.

**Mixed concern.** `alpha-flags.c` is a grab-bag covering `-mtune`,
`-msmall-data`/`-mlarge-data`, `-mfp-trap-mode` (three values),
`-mfp-rounding-mode` (three values), `-mtrap-precision=i`, `-mieee-conformant`,
`-msmall-text`/`-mlarge-text`, `-msafe-partial` and `-mlong-double-128` — flags
added across at least six different commits in this chunk, several of which
already have their own driver tests (`alpha-small-data.c`, `alpha-text-model.c`,
`alpha-fp-modes.c`, `alpha-ieee.c`). Either fold each check into the commit that
adds its flag, or introduce `alpha-flags.c` early and grow it per commit.

**Vacuous checks.** `LONGDOUBLE-NOT: error:` / `LONGDOUBLE-NOT: warning:` with no
positive `CHECK` cannot distinguish "accepted" from "clang produced no output";
likewise `LARGEDATA-NOT: "+small-data"` and `LARGETEXT-NOT: "+small-text"`.
Pair each with a positive assertion.

---

## `0443b4690da9` — [clang] Define __LONG_DOUBLE_128__ and accept -mlong-double-128

**Unverified gcc claim.** "predefine `__LONG_DOUBLE_128__` (as GCC does)".
GCC defines `__LONG_DOUBLE_128__` from `TARGET_LONG_DOUBLE_128` in the rs6000 and
s390 back ends, where long double is switchable. I can find no Alpha path that
defines it. If glibc's Alpha configure genuinely requires it, cite the configure
check instead of attributing it to gcc.

**Empty-statement branch.** `Clang.cpp`:
```cpp
else if (TC.getTriple().getArch() == llvm::Triple::alpha &&
         A->getOption().getID() == options::OPT_mlong_double_128)
  ; // Alpha's long double is 128-bit IEEE quad already; accept as a no-op.
```
A bare `;` as a statement will draw review comments; restructure the chain so the
Alpha case is an early `if (…) { /* accepted */ }` or invert the condition.

**Test in the wrong commit.** `alpha-long-double.c` only checks the macro. The
driver-acceptance half of this commit is tested by `alpha-flags.c`'s
`LONGDOUBLE` block, which was added in the *previous* commit — move it here.

---

## `ec861e1e8e2e` — [clang] Pass long double by reference

**Undocumented second change.** `classifyReturnType` also adds a blanket
"aggregates return in memory" arm:
```cpp
if (isAggregateTypeForABI(Ty))
  return getNaturalAlignIndirect(Ty, ..., /*ByVal=*/false);
```
The commit title, message and test are all about long double; nothing mentions
aggregates and nothing tests them. It is also **redundant** —
`DefaultABIInfo::classifyReturnType` already returns
`getNaturalAlignIndirect` for aggregates, and the version here *drops* the
`getRecordArgABI` handling the default has. Delete it, or split it out with a
message and a struct-return test.

**Test convention.** `clang/test/CodeGen/Alpha/long-double-abi.c` uses
`%clang --target=… -O0 -emit-llvm -S` rather than `%clang_cc1 -triple …`.
`clang/test/CodeGen` tests are `-cc1` tests by convention; going through the
driver makes them sensitive to driver defaults and to the toolchain's sysroot
probing. Same problem in `9ac03124f47e`'s `sign-extend-abi.c`.

**Comment restating code.** `// long double is passed by an invisible reference
to a caller-made copy.` directly above `getNaturalAlignIndirect(..., ByVal=true)`
inside a function whose only job is that. The `d1fe4c9bdf44` expansion of this
same comment belongs here, not two commits later.

---

## `d1fe4c9bdf44` — Fetch a _Complex va_arg from the floating-point save area

**COMMIT MESSAGE.** The final paragraph is five sentences about **a different
ticket** ("Note on the neighbouring ALPHA-020, which reported long double as
wrongly byval: that one does not reproduce…"). Tracker chatter; the useful part
of it — why `ByVal=true` is load-bearing for
`IsEligibleForTailCallOptimization` — is already in the code comment where it
belongs. Delete the paragraph and `Fixes ALPHA-019.`

**STRUCTURE.** The `classifyArgumentType` comment expansion should be squashed
into `ec861e1e8e2e`, which introduced the line it annotates.

**The fix is right.** gcc's `alpha_gimplify_va_arg_1` does recurse on the element
type for `COMPLEX_TYPE`, and each recursion rounds the element size up to 8, so
`_Complex float` consumes 16 bytes total — matching `emitSlot`'s unconditional
`+8`.

**Untested case.** The message argues specifically about `_Complex float`
("Each part takes a full 8-byte slot, so a `_Complex float` advances the offset
by 16 in total"), but `alpha-varargs.c` only adds a `_Complex double` test. Add
the `_Complex float` case — it is the one where the slot-size rounding is
observable.

**Duplication.** `emitSlot` reimplements the bias/GEP/advance sequence that the
scalar path immediately below it performs inline (the `Offset` load, the
`icmp ult 48` / `select`, the `add 8`). Use `emitSlot` for the scalar path too
and delete the duplicate.

**Worth verifying (low confidence).** A `_Complex float` part is fetched with
`load float` from a save area that `va_start` filled with `stt` (T-format). If
the FP argument registers hold S-format values that were saved as T, reading them
back as `float` is not a round trip. `float` itself is never affected (default
argument promotion), but `_Complex float` is not promoted. Worth an end-to-end
check on hardware/qemu.

---

## `1d9f5946aaa9` — Assemble mov with an immediate source and $rN register names

**Code comment is wrong.**
```cpp
// mov imm, $Rc: GNU as accepts an immediate source, materializing it in code
// (bis of a small constant, otherwise the same sequence as ldi).
```
`emitLoadImm` never emits `bis` for a small constant — it emits `lda`, which is
exactly what the test asserts (`# CHECK: lda $4, 5`). Drop the parenthetical.

**Readability.** The double negation is hard to follow:
```cpp
bool Matched = !matchRegister(("$" + Name).str(), Reg);
...
if (!Matched)
  return ParseStatus::NoMatch;
```
`matchRegister` returns `false` on success. Consider inverting `matchRegister`'s
return convention or naming the local `Failed`.

**Test.** `mov-imm-rn.s` covers exactly one immediate (`5`) and one register
pair. It does not cover `mov` with an immediate that needs the multi-instruction
path, nor `$r31`/`$r0` boundaries, nor a rejected spelling like `$r32`.

---

## `4aaeec2b8dda` — Support the v/a/b/c single-register inline-asm constraints

Register mapping matches gcc's `config/alpha/constraints.md` (`v`→R0_REG,
`a`→R24_REG, `b`→R25_REG, `c`→R27_REG). No correctness issues found.

**Clang side untested.** The `Alpha.h` `validateAsmConstraint` change has no
clang test; the only test is an LLVM CodeGen test using raw IR constraint
strings, which never exercises the front-end path that "crashed the compiler"
per the commit message. Add a `clang/test/CodeGen/Alpha/` case with
`asm("" : "=v"(x) : "a"(y))`.

**Weak assertion.** `b_and_c` checks only `bis $31, $27, $25`; it never checks
that the `=b` result is moved to `$0` for the return, which is the half that
would break if `b` were mapped to the wrong class.

---

## `f450dca2aec8` — Lower the thread-pointer intrinsics and builtin

**Spurious hunk in a core file.** The diff adds one blank line to
`llvm/include/llvm/IR/Intrinsics.td`:
```
 def int_thread_pointer : ... ClangBuiltin<"__builtin_thread_pointer">;
 
+
```
Unrelated whitespace churn in a shared header. Remove.

**CORRECTNESS: the `rduniq` read is unchained.**
```cpp
SDValue RdUniq = SDValue(DAG.getMachineNode(Alpha::RDUNIQ, DL, MVT::Glue), 0);
return DAG.getCopyFromReg(DAG.getEntryNode(), DL, Alpha::R0, MVT::i64, RdUniq);
```
`RDUNIQ` produces only a Glue result — no chain — and the `CopyFromReg` hangs off
the *entry node*, so nothing orders the read against a preceding
`llvm.alpha.set_thread_pointer` (`WRUNIQ`, which *is* chained). `hasSideEffects`
on a chainless `MachineSDNode` does not create an ordering edge. A
`set_thread_pointer(p); q = __builtin_thread_pointer();` pair can be scheduled
with the read first. Give `RDUNIQ` a chain result and thread `Op`'s chain, or at
minimum use the current chain rather than `getEntryNode()`.

**No LLVM CodeGen test.** There is no `llvm/test/CodeGen/Alpha/thread-pointer.ll`
at HEAD (`ls llvm/test/CodeGen/Alpha | grep -i thread` is empty). This commit
adds two new machine instructions, two new `LowerOperation` cases and two new
`setOperationAction` entries, and every test it adds is on the clang side. The
`call_pal 0x9e` / `call_pal 0x9f` emission is entirely unverified.

**Upstream design risk.** `__builtin_set_thread_pointer` is declared unprefixed
in `BuiltinsAlpha.td`. Nothing in this tree defines it generically today, but
AArch64/RISC-V both have a GCC builtin of that name; claiming the unprefixed
spelling for one target will collide when it is generalised. The commit message
already anticipates this ("it can move to the target-independent Intrinsics.td
once a second backend … implements it") — but that is an argument for adding it
generically now with a target predicate, not for squatting the name. Expect
pushback.

---

## `9ac03124f47e` — [clang] Extend sub-64-bit arguments and returns

**Correct.** This exactly reproduces Alpha's `PROMOTE_MODE`:
`SImode` forces `UNSIGNEDP = 0` regardless of the type; anything narrower keeps
the type's signedness. `ABIArgInfo::getExtend` picks `zeroext` for `_Bool`
because `_Bool` has unsigned representation. Good change, and the `_Bool`
rationale is real.

**Edge cases.** `extendIntegerInRegister` is reached for any
`isIntegralOrEnumerationType()` with `getTypeSize() < 64`, so `_BitInt(N)` for
non-power-of-two `N` lands in `getExtend`. Worth a test or an explicit bail.

**Commit message.** The self-host narrative ("it showed up as a clang that could
not read a source file, aborted on `-Xarch_host`, and rejected a valid
`--gcc-install-dir`") is good evidence but should be one sentence. The closing
claim is untested:
> Codegen now matches GCC instruction for instruction for every sub-64-bit
> return: cmpeq for _Bool, zapnot for the unsigned types, sll/sra for the signed
> ones, and addl for both 32-bit types.

No `llvm/test/CodeGen/Alpha` test in this commit checks any of those sequences.
Either add one or drop the claim.

**Test convention.** `%clang --target=… -emit-llvm -S` should be
`%clang_cc1 -triple …`.

---

## `da307a314e61` — Lower f128 arithmetic to the Alpha OTS runtime

**ABI is correct** against gcc's `alpha_emit_xfloating_libcall`: TFmode operands
consume register pairs starting at `$16` (so `$16/$17`, `$18/$19`), the integer
mode argument lands in `$20`, and a TFmode result comes back in `$16/$17`.

**CORRECTNESS: `Size = 8` on `OTS_CALL`.**
```
let isCall = 1, Defs = [R26, R29], isCodeGenOnly = 1, Size = 8 in
def OTS_CALL : MbrForm<0b01, (outs), (ins),
                       "jsr $$26, ($$27)\n\tldgp $$29, 0($$26)", ...
```
`JSR`, which emits the identical two-instruction sequence four lines up, is
declared `Size = 12` with the comment "Size is 4 (jsr) + 8 (the ldgp expansion,
see AlphaMCCodeEmitter::emitLdgp)". `OTS_CALL` must be 12 too — it goes through
the same `emitLdgp` path (the emitter change in this very commit adds
`case Alpha::OTS_CALL:` to the `Alpha::JSR` arm). Understated size feeds
`BranchRelaxation`. Fixed to 12 later in the series; squash back.

**CORRECTNESS: the `$20` mode argument is hardcoded.**
```cpp
// round = 2 (nearest even), required by _OtsAdd/Sub/Mul/DivX.
Chain = DAG.getCopyToReg(Chain, DL, Alpha::R20, DAG.getConstant(2, DL, MVT::i64), Glue);
```
It is not "required": gcc's `alpha_emit_xfloating_arith` passes
`alpha_compute_xfloating_mode_arg(code, alpha_fprm)`, i.e. the same
`-mfp-rounding-mode`-derived value the conversions use. This is still hardcoded
at HEAD (`AlphaISelLowering.cpp:1012-1014`), so `-mfp-rounding-mode=c` changes
`cvttq` but not `_OtsMulX`. Fix here, and correct the comment.

**Dead parameter.** `emitOtsCall(DAG, DL, Subtarget, Chain, Pv, UseRegs, Glue)`
never reads `Pv` — the procedure value reaches the call through the `$27`
register operand in `UseRegs`. All five call sites pass it. Delete the parameter.

**Three consecutive blank lines** after `LowerF128Binary` (the diff adds
`+`/`+`/`+`). `clang-format` would collapse these; the same run persists through
`0d075126ffd7` and `e9c48ebf597a`.

**Premature `setTargetDAGCombine` registration.** This commit registers combines
for `FP_EXTEND`, `FP_ROUND`, `SINT_TO_FP`, `UINT_TO_FP`, `FP_TO_SINT`,
`FP_TO_UINT`, `SETCC`, `STRICT_FSETCC`, `STRICT_FSETCCS`, `FNEG`, `FABS`,
`FCOPYSIGN` and all their strict variants, but `PerformDAGCombine` only handles
the four binary opcodes. Every FP node in every function now enters
`PerformDAGCombine` for nothing until the next three commits. Register each
opcode in the commit that handles it.

**Enum placement.** `OTS_CALL` is inserted between the memory nodes and
`LAST_MEMORY_OPCODE = SAFE_USTORE`, so it silently sits outside the memory range
by one. It is not a memory opcode; move it out of that block entirely.

**No CALLSEQ.** The `.td` comment asserts "no CALLSEQ is needed". That is true
only because all arguments are in registers; note that `MachineFrameInfo`'s
`AdjustsStack` is consequently never set for these calls, which is a latent
assumption for anything that later reads it.

**Codegen quality.** Every operand goes through a fresh 16-byte stack slot
(`splitF128` creates a new `CreateStackObject` each call) and every result goes
through another (`joinF128`), so a chain of f128 adds does store/load/store/load
between each pair of calls. gcc keeps the halves in registers. Worth a note in
the commit message as known-suboptimal, since nothing in the tests reveals it.

**Tests.** `f128-arith.ll` (and `f128-convert.ll`, `f128-compare.ll`,
`f128-bitwise.ll`) use the deprecated `-march=alpha` plus an in-file
`target triple`; the rest of the series uses `-mtriple=`. More importantly, the
tests check only `ldq $27, _OtsAddX($29)` and `jsr $26, ($27)` — the *symbol*.
The header comment spells out the ABI ("lo/hi halves of each f128 in $16-$19, the
round constant (2 = nearest) in $20, and returns the result lo/hi in $16/$17")
and **not one of those registers is checked**. Every ABI detail that could be
wrong is invisible to this test. Add checks for the `$16`–`$20` setup and the
`$16`/`$17` result reads.

---

## `0d075126ffd7` — Convert between f128 and the other types through OTS

**COMMIT MESSAGE DESCRIBES CODE THAT IS NOT HERE.**
> f128 to f32 goes through f64: the runtime exports no X-to-S routine, and gcc
> emits the same pair.

The `FP_ROUND` arm bails unless the result is `f64`, and the `FP_EXTEND` arm
bails unless the source is `f64`; `f32`↔`f128` is unhandled and falls through to
generic soft-float. The f32 support arrives in commit #194,
`0a630862b917 [Alpha] Convert between f32 and f128`. Move the paragraph there
(or squash the two commits).

**Routine selection matches gcc**, including the surprising bits: `FIX` and
`UNSIGNED_FIX` both use `_OtsCvtXQ` (gcc's `alpha_emit_xfloating_cvt` does
`if (code == UNSIGNED_FIX) code = FIX;`), `FLOAT`→`_OtsCvtQX`,
`UNSIGNED_FLOAT`→`_OtsCvtQUX`, and `FLOAT_EXTEND`/`FLOAT`/`UNSIGNED_FLOAT` take
no mode argument while `FIX`/`FLOAT_TRUNCATE` do. Result registers (`$16/$17` for
TF, `$f0` for DF, `$0` for DI) match too.

Do note in the commit message that `fptoui fp128` inherits gcc's limitation:
`_OtsCvtXQ` is a signed conversion, so values ≥ 2^63 are wrong. Right now the
code silently routes `FP_TO_UINT` there with no comment.

**Wide-type crashes.** Both integer arms assume the non-f128 side is ≤ 64 bits:
```cpp
if (Src.getSimpleValueType() != MVT::i64)
  Src = DAG.getNode(IsUnsigned ? ISD::ZERO_EXTEND : ISD::SIGN_EXTEND, DL, MVT::i64, Src);
```
and
```cpp
if (ResVT != MVT::i64)
  Result = DAG.getNode(ISD::TRUNCATE, DL, ResVT, Result);
```
`sitofp i128 to fp128` builds `SIGN_EXTEND i128 -> i64` and `fptosi fp128 to i128`
builds `TRUNCATE i64 -> i128`; both assert in `getNode`. Guard on
`getSizeInBits() < 64` / `> 64` and bail. (HEAD has comments acknowledging this
for `_OtsCvtQ[U]X`; the guard should be here.)

**Test.** Four cases, each checking only the `ldq $27, _Ots…($29)` literal load.
No signed/unsigned distinction is tested (`_OtsCvtQUX` never appears in the
test), the `$18` mode argument is not checked (that comes in the *next* commit),
and no strict variant is covered even though every arm has strict handling.

---

## `f94dcafc5813` — Compute the OTS X_floating mode argument the way gcc does

**Correct as far as it goes**, and the port of
`alpha_compute_xfloating_mode_arg` is faithful: chopped 0 / minus 1 / nearest 2 /
dynamic 4, and `|= 0x10000` for `FLOAT_TRUNCATE` when `alpha_fptm == ALPHA_FPTM_N`
(spelled here as `!FeatureIEEE && !FeatureFPTrapU`). `FIX` correctly passes
`FPRoundChopped` unconditionally, matching gcc's hardcoded `ALPHA_FPRM_CHOP`.

**But the title overpromises.** Only the two *conversion* sites are fixed. The
`$20` argument in `LowerF128Binary` remains `getConstant(2, …)` (see
`da307a314e61` above), so "the OTS X_floating mode argument" is still hardcoded
for add/sub/mul/div. Either fix all three sites here or retitle.

**STRUCTURE: squash into `0d075126ffd7`.** The hardcoded 2 was introduced one
commit earlier; there is no reason to ship the wrong constant and then fix it.

`Fixes ALPHA-003.` — drop.

**Test.** `f128-round-mode.ll` is the best test in the f128 group — it actually
checks the immediate materialised into `$18` across five subtarget
configurations. It should be the model for `f128-arith.ll` and
`f128-convert.ll`. Missing: a case for the arithmetic `$20` argument (which
would currently fail, per above).

---

## `e9c48ebf597a` — Compare f128 values through OTS

**CORRECTNESS (highest severity in this chunk): the OTS return value is
tri-state, not boolean.** gcc `alpha_emit_xfloating_compare` documents it:

> X_floating library comparison functions return
>   −1 unordered, 0 false, 1 true.
> Convert the compare against the raw return value.

and then selects a *comparison* against zero per condition:
`UNORDERED` → call `_OtsEqlX`, test `< 0`; `ORDERED` → `_OtsEqlX`, test `>= 0`;
`NE` → `_OtsNeqX`, test `!= 0`; `EQ/LT/GT/LE/GE` → the matching routine, test
`> 0`.

This commit instead does:
```cpp
SDValue Result = DAG.getCopyFromReg(Chain, DL, Alpha::R0, MVT::i64, Glue);
if (Negate)
  Result = DAG.getNode(ISD::XOR, DL, MVT::i64, Result, DAG.getConstant(1, DL, MVT::i64));
MVT SetCCVT = N->getSimpleValueType(0);
if (SetCCVT != MVT::i64)
  Result = DAG.getNode(ISD::TRUNCATE, DL, SetCCVT, Result);
```
Consequences with a NaN operand (return value −1):
* `fcmp olt` → `trunc i64 -1 to i1` = **1**; should be 0.
* `fcmp une` → `(-1 XOR 1) = -2`, `trunc to i1` = **0**; should be 1.
* every ordered predicate is wrong for NaN, and every negated one is wrong twice.

Replace the truncate with an explicit `SETGT Result, 0` (and `SETLT`/`SETGE` for
the unordered/ordered predicates once they are implemented).

**CORRECTNESS: `SETO`/`SETUO` are silently unhandled.** The switch's `default:`
returns `SDValue()`, so `fcmp ord`/`fcmp uno` on f128 fall through to generic
soft-float — and for `STRICT_FSETCC` on f128 there is no `setOperationAction`
entry at all (see `cf27fe3719cc`), so a constrained `ord`/`uno` reaches
legalization with no action. The commit message claims otherwise:
> and the unordered forms by testing for a NaN first.

There is no NaN test anywhere in the function. Either implement gcc's
`UNORDERED`/`ORDERED` arms (call `_OtsEqlX`, test `< 0` / `>= 0`) or delete the
claim and document the gap.

**Predicate table.** The `!ordered-opposite` identities are individually correct
(`SETUGT = !SETOLE`, etc.) *given* a boolean result — but see above; with the
tri-state return they are not.

Also note that `SETONE → _OtsNeqX` with a truthiness test is semantically
`SETUNE` in gcc (gcc's `NE` case tests `!= 0`, which counts −1 as "not equal"),
so the ordered/unordered assignment of `_OtsNeqX` needs rechecking once the
comparison against zero is added.

**Inconsistent chain handling.** This function passes `InChain` as the store
chain into `splitF128` and omits `InChain` from the `TokenFactor`, while
`LowerF128Binary`/`LowerF128Convert` pass `getEntryNode()` and include `InChain`
in the `TokenFactor`. Both work; pick one.

**Tests.** Every case checks only `ldq $27, _OtsXxxX($29)`. Nothing checks the
result handling, so none of the bugs above are detectable. No `ord`/`uno` case.
The strict case (`une_f128_strict`) checks the same two lines as the non-strict
one, so it cannot distinguish "chain threaded" from "chain dropped" — contrary to
its own comment "chain threaded".

---

## `04341794f7a7` — Do f128 sign operations without a call

**CORRECTNESS: `FCOPYSIGN` operand types may differ.** `ISD::FCOPYSIGN` permits
operand 1 to have a different floating type from operand 0 (DAGCombine creates
such nodes when folding an `fpext`/`fptrunc` into the sign operand). The code
unconditionally `splitF128`s `N->getOperand(1)`, so an `f64` sign operand would
be stored into a 16-byte slot and its uninitialised high half read as the sign
word. Add
`if (N->getOperand(1).getSimpleValueType() != MVT::f128) return SDValue();`.

**Convoluted mask.** `~signmask` is built as a DAG node:
```cpp
DAG.getNode(ISD::XOR, DL, MVT::i64, SignMask, DAG.getConstant(-1ULL, DL, MVT::i64))
```
twice. Use `DAG.getConstant(APInt::getSignedMaxValue(64), DL, MVT::i64)` — one
node, no reliance on the combiner folding it.

**`auto [_, SignHi]`** — `_` as a structured-binding name is unusual in LLVM and
some builds warn on it. Prefer a named-but-unused binding or restructure.

**Round-tripping through memory for a sign flip.** `splitF128` stores the f128 to
a stack slot and loads two i64s; `joinF128` stores them back and loads an f128.
For `fneg` that is store/load/xor/store/load where gcc emits a single `xor` on
the high register. The tests cannot see this because they check only
`CHECK-NOT: jsr` and `CHECK: xor`.

**Tests.** `fcopysign_f128` has **no positive check at all** after its label —
only `CHECK-NOT: jsr`. It would pass on any implementation that does not call a
function, including a wrong one. `fabs_f128`'s `CHECK-NOT: jsr` only covers the
range up to the first `and`, so a `jsr` after it would not be caught. Add
`CHECK-NOT: jsr` *after* the last positive check in each function, and check the
actual and/or sequence in `fcopysign_f128`.

---

## `af4734b01f9e` — [clang] Advertise lock-free atomics

No correctness issues. `MaxAtomicInlineWidth = 64` with sub-word CAS expanded to
a masked longword loop is consistent with the rest of the backend, and defining
the four `__GCC_HAVE_SYNC_COMPARE_AND_SWAP_*` macros in `getTargetDefines` is the
established per-target idiom (SystemZ, LoongArch, M68k all do it).

Minor: the code comment duplicates the commit message and the test's header
comment word for word ("ldl_l/stl_c and ldq_l/stq_c give native 4- and 8-byte
compare-and-swap, and the sub-word cases are expanded to a masked longword loop,
so every size … is lock-free and inlined"). Keep it in one place.

---

## `46782716b106` — [clang] Advertise strict floating point

Four added lines, no test on the clang side (no `-frounding-math`/
`-ftrapping-math` acceptance test, no `__STDC_IEC_559__`-style check).

**Commit-message convention.** The last paragraph is series meta-commentary:
> This lands after the f128 lowering, since constrained floating point on long
> double reaches the X_floating paths.

Ordering rationale belongs in the cover letter / review thread, not in the
permanent commit message.

**STRUCTURE.** This and `cf27fe3719cc` are the two halves of "make constrained FP
work on Alpha" and are already adjacent — good. Consider combining them so the
`HasStrictFP` flip and the `STRICT_FSETCC` lowering that makes it not crash land
together (as committed, `46782716b106` alone turns on a path that crashes until
the next commit).

---

## `cf27fe3719cc` — Custom-lower STRICT_FSETCC/STRICT_FSETCCS for f32 and f64

**Semantics.** Lowering both `STRICT_FSETCC` (quiet) and `STRICT_FSETCCS`
(signaling) to a plain `SETCC` collapses a distinction Alpha *can* express: the
`/su`-qualified compares raise invalid on a quiet NaN and the unqualified ones do
not. The commit message's justification —
> Alpha has no per-instruction FP-exception signaling: floating-point exception
> enable/disable is a global mode bit
— conflates *enabling* a trap (global, in the FPCR) with *raising* the exception
flag (per-instruction, via the `/S`/`/U` qualifiers already implemented in
`96c00ba31848`). Reviewers will push on this. At minimum, note that under
`-mieee` the compares already get `/su` and so *do* signal, which contradicts the
stated premise.

**Gap this leaves.** The comment says "f128 is handled in PerformDAGCombine
before legalization", but `LowerF128Compare` returns `SDValue()` for `SETO` and
`SETUO`, so a constrained `ord`/`uno` on f128 reaches legalization with no
action registered for f128 either — the exact crash this commit exists to
prevent, just for a different condition code.

**Commit-message formatting.** Wrapped at ~62 columns with a word hyphenated
across a line break ("only the (unimple-\nmentable) per-instruction trap is
dropped"). LLVM wraps at 72–80 and does not hyphenate.

**Test.** Two cases. `strict_olt_f64` uses `constrained.fcmp` and
`strict_oeq_f32` uses `constrained.fcmps`, so neither type is exercised with both
intrinsics — and since both lower identically, the test cannot detect the two
being conflated. Add a `-mattr=+ieee` run to show what the qualifier does to
these compares.

---

## `ff9e7a19f6c7` — [clang] Map -Wa,-mevN assembler ISA flags to target features

**CORRECTNESS: `-mpca56` is missing MVI.**
```cpp
if (Value == "-mev56" || Value == "-mpca56") {
  CmdArgs.push_back("-target-feature");
  CmdArgs.push_back("+bwx");
  continue;
}
```
The backend's own CPU table (`Alpha.td`) has
`def : ProcessorModel<"pca56", Alpha21164Model, [FeatureBWX, FeatureMVI]>`, and
PCA56 is the part that *introduced* MVI. `-Wa,-mpca56` should push `+bwx` and
`+mvi`; as written it silently downgrades to EV56.

**Missing spellings.** gas also accepts `-mev45`, `-mev68` and `-mall`. `-mev45`
would fall through to the generic unknown-argument handling.

**Test.** Only `-mev6` is exercised, with three `CHECK-DAG` lines and no
`CHECK-NOT: "+cix"` to prove EV6 does not get CIX. `-mev4`, `-mev5`, `-mev56`,
`-mpca56` (the buggy one) and `-mev67` are all untested. Uses the deprecated
`-target alpha-linux-gnu`.

**STRUCTURE: REORDER.** This is a self-contained clang-driver commit that has
nothing to do with the floating-point work it currently sits at the end of. Move
it next to the other `-mcpu`/ISA-feature driver commits.
