# Review: commits 101–130 of the Alpha backend series

Range: `cd617a876f3a` .. `44a29238297d` (oldest first), on `alpha-triple-0bf3638`.

## Summary

The code in this chunk is mostly sound: I checked every opcode/function-field
encoding introduced here (cmpbge, zap/zapnot, the full ext/ins/msk family, MVI,
amask/implver/rpcc, itoft/ftoit/itofs/ftois, addl/subl/mull, s4addl..s8subl,
prefetch loads, mf_fpcr/mt_fpcr, the IEEE trap-qualifier deltas) against the
Alpha ARM and they are all correct, and the misaligned ldq_u/ext/ins/msk
sequences and the "8*n mod 64 == 8*(n mod 8)" variable-position patterns are
correct for every alignment.

The problems cluster in four places:

1. **Two real correctness bugs.** `0a0690123ab2` accepts `ret`/`jmp` with
   *any* four operands and silently discards them, so `jmp $16, ($27), 0`
   assembles as `jmp $31, ...` — a silent wrong-code bug in hand-written
   assembly, which is exactly what that commit exists to support.
   `b6fbd19b4eeb` emits `.arch ev6` for CIX code; GNU as requires `ev67` for
   CIX, so externally-assembled ctpop/ctlz/cttz output will be rejected (and
   the parser side mirrors the same wrong mapping).
2. **A dead scheduling model.** The EV6 `Rd_FStData` read-advance and the
   `Wr_ItoF`/`Wr_FtoI` classes are attached to `MOVi2f`/`MOVf2i`, which are
   `usesCustomInserter` pseudos expanded before the machine scheduler ever
   runs. Every InstRW entry for them is dead, and the instructions they expand
   to (ITOFT/FTOIT/ITOFS/FTOIS, or the stq/ldt pair) have no InstRW at all.
   `01b5aa0ae6f1`'s commit message reasons at length about this dead path.
   Later commits in the chunk add MULL/SUBL/ADDLi/S4ADDL/EXTBLi/... without
   ever touching `AlphaSchedule.td`.
3. **Known-bad designs fixed later in the series.** `bb58f0db8237` puts
   `-mieee` policy in the MC layer; `9ca05e38507d` (outside this chunk) undoes
   it. `0e0d39c89651` ships incomplete InstRW lists; `4fa520dcb620` completes
   them. `0e0d39c89651` sets `MispredictPenalty = 7` and `01b5aa0ae6f1`
   changes it to 11 four commits later. All three should be squashed backward.
4. **Systematic LLM-tells.** Three commits carry reviewer-directed
   meta-justification for their own granularity ("Keeping the three in one
   commit is deliberate…", "since they share one definition class and
   splitting them meant rewriting…"). Several commit messages end with an
   orphan paragraph that reads as an `--amend` append rather than prose.
   `01b5aa0ae6f1` commits a literal `???` marker and a lab-notebook comment
   into `AlphaSchedule.td`. Two test files embed series-history narration
   ("it omitted prefetch and precise arithmetic traps") that is meaningless
   upstream. Test-file header paragraphs frequently restate the commit message
   verbatim.

Test quality is the weakest dimension: `schedule.ll` cannot fail,
`atomic-subword-nobwx.ll`'s `-NOT` lines cannot fail by construction,
`ieee.ll` has two prefix-matching CHECKs that cannot fail, `large-frame.ll`
does not test the `eliminateFrameIndex` path its commit message describes,
and two commits (`45dfd5063fae`, `327dd5f881db`) *delete* existing coverage
of paths they keep.

---

## cd617a876f3a — [Alpha] Support a frame pointer and dynamic stack allocation

**Structure.** Three concerns in one commit: DYNAMIC_STACKALLOC + frame
pointer, the `hasReservedCallFrame` override, and stacksave/stackrestore. The
commit message reflects this: it has a body, a "Validated under qemu-alpha"
sign-off paragraph, and then a *further* paragraph after the sign-off
("stacksave/stackrestore read and write $30 directly…"). Text after the
validation/attribution paragraph reads as an amend append. Fold it into the
body or split the commit.

I verified the `hasReservedCallFrame` rationale: `TargetFrameLowering`'s
default is `return !hasFP(MF);` (TargetFrameLowering.h:314), so the override
is not a no-op and the commit message is accurate.

**Bugs / gaps.**

- `AlphaFrameLowering.cpp` — the frame-pointer save/restore instructions carry
  no frame flags. The prologue `STQ $15` and `copyReg(R15, R30)` get no
  `MachineInstr::FrameSetup`; the epilogue `copyReg(R30, R15)` and `LDQ $15`
  get no `FrameDestroy`. (The prologue STQ gains `FrameSetup` in the *next*
  commit, `f7ef85d7b231`; the epilogue instructions never do.) These flags
  drive `skipFrameInstrs` itself, shrink-wrapping, and the CFIFixup pass added
  two commits later. Add them here.
- `skipFrameInstrs` declares `bool IsSetup` but both call sites write
  `/*Setup=*/true` — the inline argument comment does not match the parameter
  name (`bugprone-argument-comment`).
- `LowerDYNAMIC_STACKALLOC` hardcodes `15` / `-16` for the stack alignment
  instead of deriving them from `getStackAlign()`, which is stated as 16 in
  the same commit's `eliminateCallFramePseudoInstr` (`alignTo(...,
  getStackAlign())`). One of the two should follow the other.

**LLM-tells.**

- `// Copy one integer register to another with \`bis $31, Src, Dst\`.` above
  `copyReg` — restates the single BuildMI below it.
- `call-frame-alloca.ll:3-7` and `dynamic-alloca.ll:3-6` open with a paragraph
  that is a near-verbatim copy of the commit message. Test headers should say
  what invariant the test pins, not re-narrate the change.
- `dynamic-alloca.ll` and `stacksave.ll` both name their function `f`. Commit
  `4790ba4c5343` in this same series is titled "[Alpha] Drop the tutorial
  preambles and name the test functions"; these two files violate the
  convention that commit established.

---

## f7ef85d7b231 — [Alpha] Emit DWARF call-frame information

**Bug: the prologue CFI is not async-precise.** `Pos` is computed as
`skipFrameInstrs(MBBI, MBB, /*Setup=*/true)`, i.e. *after* the callee-save
spills, and every CFI directive — including
`cfiDefCfaOffset(nullptr, StackSize)` — is emitted there. The emitted order is:

```
lda  $30, -N($30)      <-- CFA is already wrong from here
stq  $26, ...($30)
stq  $15, ...($30)
bis  $31, $30, $15
.cfi_def_cfa_offset N  <-- only now correct
.cfi_offset $26, ...
```

A signal landing between the `lda` and the first directive unwinds with a CFA
that is N bytes off. This is precisely the property the *next* commit
(`3037704af620`) goes to great lengths to guarantee in the epilogue ("so the
state is correct at every instruction"). `cfiDefCfaOffset` must be emitted
immediately after the stack adjustment, and each `.cfi_offset` immediately
after its spill.

**Tests.**

- `cfi.ll:11` names the first function `leaf_caller`, but it calls `@g` — it
  is not a leaf. Rename (`caller`, `simple_frame`).
- `; CHECK: .cfi_offset $26,` — the trailing comma with no operand means the
  saved-register offset is never checked. Same for `$15`. Pin the values;
  a wrong offset is exactly the bug class this commit fixes.
- `with_fp` has no `.cfi_def_cfa_offset` *value* check either.

---

## 3037704af620 — [Alpha] Emit epilogue call-frame information

**Bug: `resetCFIToInitialState` emits its directives in reverse.**

```cpp
auto emitCFI = [&](const MCCFIInstruction &Inst) {
  ...
  BuildMI(MBB, MBB.begin(), DL, ...)
```

`MBB.begin()` is re-evaluated on every call and each insertion goes *before*
the current first instruction, so the last directive emitted ends up first.
`def_cfa` and the `same_value` list are order-independent so the output is
still correct, but the code does not do what it reads as. Hoist
`MachineBasicBlock::iterator I = MBB.begin();` out of the lambda.

**Fragility.** The callee-save restore loop finds "the last definition of the
register before the terminator". If shrink-wrapping or a tail-merge puts the
reload in a different block, or a call in this block re-defines `$26` after the
reload, the `.cfi_restore` attaches to the wrong instruction with no
diagnostic. Consider keying off `MachineInstr::FrameDestroy` + the CSI frame
index instead of a def scan.

**Testing.** The `setCFIFixup(true)` / `resetCFIToInitialState` half of this
commit — the part the third paragraph of the message is entirely about — has
no test. `cfi.ll` gains two lines that only exercise the straight-line
epilogue. A function with a live-frame block laid out after an early-return
epilogue should be added, checking for `.cfi_remember_state` /
`.cfi_restore_state` or the reset sequence.

---

## 9c4ba65e4603 — [Alpha] Support stack frames larger than 32KiB

Encoding logic is correct, including the `Hi == 0x8000` → two `ldah` of
`0x4000` split (the only value of `Hi` for which `isInt<16>` fails, since
`Hi ∈ [-0x8000, 0x8000]` for a 32-bit offset).

**Testing.** The commit message describes two mechanisms; the test covers one.
`large-frame.ll` checks only the prologue/epilogue `lda`+`ldah`+`addq`
sequence. The `eliminateFrameIndex` path ("a frame slot beyond 16 bits
materializes the high part of its offset into $28 with ldah") has **no CHECK
line at all**, and the `Hi == 0x8000` double-`ldah` branch in
`AlphaRegisterInfo.cpp` is unreachable from this test (it needs an offset
≥ 0x7FFF8000, i.e. a ~2GiB frame). Add a case that pins
`ldah $28, N($30)` + `ldq $x, M($28)`.

Also, every immediate in the test is `{{-?[0-9]+}}`, so the test cannot
distinguish a correct decomposition from a wrong one. Since `[5000 x i64]` is
a fixed size, the exact `lda`/`ldah` operands are deterministic — pin them.

---

## 0a0690123ab2 — [Alpha] Assemble hand-written context-save assembly

**Correctness bug (silent wrong code).** In `matchAndEmitInstruction`:

```cpp
if (Mnemonic == "ret" && Operands.size() == 4) {
  MCInst Inst;
  Inst.setOpcode(Alpha::RET);
  ...
}
if (Mnemonic == "jmp" && Operands.size() == 4) {
  Inst.setOpcode(Alpha::JMP);
  Inst.addOperand(MCOperand::createReg(
      static_cast<AlphaOperand &>(*Operands[2]).getMemBase()));
```

Neither branch validates anything but the operand *count*:

- `ret $16, ($26), 1` assembles as `ret $31, ($26), 1`, silently dropping the
  link-register write.
- `jmp $16, ($27), 0` likewise loses `$16` — the whole point of `jmp` with a
  non-`$31` Ra is that it writes the return address.
- `ret $31, $26, 1` (no parens) reaches `getMemBase()` on an operand that is
  not a memory operand; there is no `isMem()` guard.
- The hint operand (4th) is ignored rather than encoded, so `jmp $31, ($27),
  1` and `jmp $31, ($27), 0` produce the same word.

Check that operand 1 is `$31` (`$26` for `ret`'s Rb), that operand 2 is a
memory operand, and either encode or reject the hint — otherwise refuse to
match. Silently miscompiling hand-written assembly is worse than the
`-fno-integrated-as` fallback this commit removes.

**Commit message.** Bulleted body, and a trailing paragraph after the
"With these, libunwind's three assembly files assemble…" conclusion that adds
a fourth, unlisted change (the disassembler `decodeFpcrMove`). It reads as an
amend append; it belongs in the bullet list.

**Nit.** `AlphaInstrInfo.td` gains two consecutive blank lines after the
`MT_FPCR` block.

**Test.** `pseudo-asm.s` is a good MC test, but the `.set` handling has no
negative case: `.set foo, 1` must still reach the generic parser, and
`.set noat` in an unexpected position should not be silently eaten. Add one
line proving symbol assignment still works.

---

## 2f67d49efc2e — [Alpha] Add integer and floating-point negate/copy pseudo aliases

Semantics verified: `fabs` → `cpys $f31, Rb` (sign from `$f31` = positive,
magnitude from Rb) ✓; `fmov`/`fneg` → `cpys`/`cpysn Rb, Rb` ✓;
`sextl` → `addl $31, Rb` ✓; `negs`/`negt` → `subs/subt $f31, Rb` matches GAS.

**Test.** The commit message claims "Each is byte-identical to the GNU
assembler's output", but the ten new CHECK lines in `pseudo-asm.s` check only
the *printed* base instruction — none has `# encoding:`, unlike the six lines
directly above them in the same file. Nothing in the test verifies the bytes.
Add `# encoding:` to at least the non-obvious ones (`negq`, `sextl`, `fabs`).

---

## 1af22d79e410 — [Alpha] Add the llvm.alpha.* intrinsics

All 45 opcode/function pairs verified correct against the ARM (base,
ext/ins/msk high and low, MVI 0x1C.31–0x3F, amask 0x11.61, implver 0x11.6C
with LIT=1, rpcc 0x18 func 0xC000).

**Bug: the CIX intrinsic patterns are not feature-gated.**

```tablegen
// The count intrinsics reuse the CIX instructions (also the generic ctpop/
// ctlz/cttz patterns), so they select only when the cix feature is enabled.
def : Pat<(int_alpha_ctpop i64:$x), (CTPOP GPRC:$x)>;
```

The comment is wrong. A standalone `def : Pat<>` takes its predicates only
from an enclosing `let Predicates = [...]` block; it does not inherit them from
the instruction it emits. With `-mcpu=ev4`, IR containing `llvm.alpha.ctpop`
selects `CTPOP` and emits an instruction the target cannot execute. Contrast
the MVI instructions, whose intrinsic patterns are inline in defs inside
`let Predicates = [HasMVI]` and *are* gated. Wrap these three in
`let Predicates = [HasCIX]`.

**Commit message.** The first paragraph lists seven intrinsics; the third says
"This covers the whole set … the extract/insert/mask family, the count
instructions and the motion video instructions". These contradict each other —
the first paragraph should list the whole set or say "the set". More
importantly, the third paragraph's justification — "since they share one
definition class and splitting them meant rewriting the first half's
definitions rather than adding to them" — is reviewer-directed
meta-commentary about the commit's own granularity. It does not belong in an
upstream commit message. Same pattern recurs in `0e0d39c89651`.

**Style.** Line 254 of `AlphaInstrInfo.td` (`// Motion video instructions (MVI,
enabled by the mvi feature): packed byte/word min and max, the sum`) is 101
columns. The series contains `c9a6ae54bb9d "[Alpha] Format the series with
clang-format"`; clang-format does not reflow comments, so this one slipped.

**Tests.** `intrinsics.ll` covers 13 of ~45 intrinsics. Untested: every
`def : Pat<>` added at the bottom of the file — `int_alpha_umulh`,
`extbl/extwl/insbl/inswl/mskbl/mskwl` (the routed low forms), `ctlz`, `cttz`
— i.e. exactly the hand-written patterns most likely to be wrong. `umulh` is
tested but via the intrinsic, which is the routed pattern, so that one is
covered.

---

## e3cca3af6f8b — [clang][Alpha] Implement the __builtin_alpha_* functions

**Commit message contradicts the diff.** Final paragraph:

> -mcpu does not imply these features yet, so reaching a CIX or MVI builtin
> takes the -m flag spelled out. Deriving the features from the processor
> comes later.

But this very diff adds `AlphaTargetInfo::initFeatureMap` mapping
`ev56→bwx`, `pca56→bwx,mvi`, `ev6→bwx,mvi,fix`, `ev67→+cix`. The paragraph
is stale and must be deleted. The message also never mentions `initFeatureMap`,
`setCPU`, or the `getCPUName` change in `CommonArgs.cpp`, which are the
non-obvious parts.

**Dead member.** `std::string CPU = "generic";` and `setCPU()` store the CPU
name, but nothing ever reads `CPU` — `initFeatureMap` receives the CPU as a
parameter. Also, `setCPU` assigns before validating:

```cpp
bool setCPU(StringRef Name) override { CPU = Name.str(); return isValidCPUName(Name); }
```

so an invalid CPU is still recorded. Drop the member and the override, or
validate first.

**Test-file narration.** `clang/test/Driver/alpha-mcpu.c:1-5`:

> The driver must not push them as -target-feature as well: that duplicates
> the mapping in a second place, where it drifts (it omitted prefetch and
> precise arithmetic traps) without changing anything.

This describes a state of *this branch's history*, not of upstream LLVM. A
reader of the merged tree has no way to know what "it omitted" refers to (and
`FeaturePrefetch` does not even exist until commit `37b6ead2f875`, later).
Rewrite as a statement of the invariant being tested.

**Fragile test.** `alpha-builtins-features.c` runs the same file twice, once
with `+cix +mvi` and once with `-verify`, and the comment says:

> Code generation stops at the first such builtin, so only one error is
> checked here.

But the file uses gated builtins in five more functions (`test_ctpop`,
`test_ctlz`, `test_minub8`, `test_perr`, `test_unpkbw`), each of which is a
candidate `err_builtin_needs_feature`. The test passing depends on a CodeGen
bail-out that is not a documented guarantee. Split the `-verify` run into its
own file with exactly one gated builtin.

**Nit.** `BuiltinsAlpha.inc` is appended after SystemZ in
`clang/include/clang/Basic/CMakeLists.txt` rather than in the surrounding
alphabetical position.

---

## 104c1f65267d — [Alpha] Lower misaligned loads and stores with ldq_u and extract/insert

The sequences are correct. I verified that the load form works at *every*
alignment including 0 (`EXTxH` with `byte_loc == 0` is a mathematical shift by
64, i.e. 0, so the `ldq_u` of `Ptr+7` contributes nothing when `Ptr` is
already aligned), and that the store's msk/ins/or/stq_u ordering matches the
canonical ARM sequence with the correct chain serialization.

**Gaps.**

- **Floating-point misaligned access is not handled.** Only `ISD::LOAD`/
  `ISD::STORE` on `MVT::i64` and the i16/i32 ext/trunc actions are made
  Custom. A `load double, ptr %p, align 1` still expands byte-by-byte —
  the exact pathology the commit exists to fix. Either bitcast f32/f64 through
  the integer path or say why not.
- The `MachineMemOperand`s claim `Align(8)` at `Ptr` with size 8, but `ldq_u`
  actually touches `[Ptr & ~7, (Ptr & ~7)+8)`, which extends *below* `Ptr`.
  `MachinePointerInfo()` is unknown so aliasing stays conservative, but the
  alignment claim on the unmasked pointer is a lie that an alignment-based
  transform could act on.

**Structure.** `AlphaSelectionDAGInfo` is defined inline in
`AlphaISelLowering.h`. Every other target puts this in
`<Target>SelectionDAGInfo.h/.cpp`. Also the new
`#include "llvm/CodeGen/SelectionDAGTargetInfo.h"` is placed *after*
`TargetLowering.h`, breaking the sorted include block.

**Tests (`unaligned.ll`).**

- No sextload case, though the commit message calls it out ("a sign-extending
  load adds a sign_extend_inreg"). `load_l`/`load_w` return the value directly
  so they are any/zero-extending. Add `sext i16 → i64`.
- `store_q` has one `CHECK-DAG: stq_u` for two emitted stores, and no check
  for the two `ldq_u` reads of the read-modify-write.
- No misaligned *store* of a word or longword (`mskwl/inswl`, `mskll/insll`) —
  only the quadword store is checked, so half of `getUnalignedOps` is untested.
- The header paragraph restates the commit message.

---

## 0e0d39c89651 — [Alpha] Add scheduling models for the 21064, 21164 and 21264/21364

**Commit message.** The body is written EV6-first and then says "The 21064
(EV4) and 21164 (EV5) models are here too", which is the reverse of the title
order and reads as an amend append. The final paragraph —

> Keeping the three in one commit is deliberate: they share the bypass and
> operand-cycle vocabulary, and separating them meant rewriting most of one
> model in the next commit.

— is meta-commentary about the commit's own granularity addressed to a
reviewer. Delete it (same tell as `1af22d79e410`).

**Dead scheduling entries.** `MOVi2f`/`MOVf2i` are `usesCustomInserter`
pseudos, expanded by `EmitInstrWithCustomInserter` during the Finalize-ISel
pass, i.e. *before* the machine scheduler. Therefore

```tablegen
def : InstRW<[Wr_FtoI, Rd_FStData], (instrs MOVf2i)>;
def : InstRW<[Wr_ItoF], (instrs MOVi2f)>;
```

never apply to anything. The EV6 `Rd_FStData` advance for
`Wr_FAdd/Wr_FMul/Wr_FCmov` producers therefore only affects FP stores, not
float-to-integer moves — and commit `01b5aa0ae6f1`'s commit message reasons
extensively about exactly this dead path ("MOVf2i reads its source through
Rd_FStData, which already carries the -2 advance"). Model the real
instructions instead: `ITOFT`/`FTOIT`/`ITOFS`/`FTOIS` and the `STQ`+`LDT` /
`STT`+`LDQ` pairs.

**Incomplete instruction coverage.** `CompleteModel = 0` hides this, but the
map already misses instructions that exist at this commit (`CMOVLE`/`CMOVGE`/
`CMOVLBC`/`CMOVLBS` if defined, `TRAPB`/`EXCB`, the div millicode pseudos,
`AMASK` is mapped as `Wr_IALU` though `amask` is not a 1-cycle ALU op on all
implementations). Worse, later commits *in this chunk* add `MULL`, `SUBL`,
`ADDLi`/`SUBLi`/`MULLi`, `S4ADDL`/`S8ADDL`/`S4SUBL`/`S8SUBL`,
`EXTBLi`/`EXTWLi`/`EXTLLi`, `ITOFS`/`FTOIS`, `PREFR`/`PREFREN`/`PREFW`/
`PREFWEN`, `MOVi2f_S`/`MOVf2i_S` — none of which touches `AlphaSchedule.td`.
`MULL` in particular then schedules as an unmodeled instruction rather than a
7-cycle multiply. The out-of-chunk commit `4fa520dcb620 "[Alpha] Complete the
scheduling models"` fixes this; that fix should be squashed backwards into
this commit and into each commit that adds an instruction.

**Test `schedule.ll` cannot fail.** Three RUN lines (`ev4`, `ev5`, `ev6`) all
use the same `CHECK` prefix, so the test asserts all three models produce the
*same* output — which defeats the purpose of having three models. And the
checks are:

```
; CHECK-DAG: mulq
; CHECK-DAG: addq $19, $20,
```

`CHECK-DAG` imposes no ordering, and both instructions are emitted regardless
of scheduling, so the stated property ("independent work is interleaved with
the long integer multiply") is not tested at all. Use per-CPU prefixes with
ordered `CHECK`/`CHECK-NEXT` on the interleaving, or `llvm-mca`.

---

## 01b5aa0ae6f1 — [Alpha] Refine the EV6 mispredict and cross-cluster modeling

**Commit message is factually wrong.** "The EV4 and EV5 models are unchanged."
The diff adds `def : ReadAdvance<Rd_IMVIOp, 0>;` to both the EV4 and the EV5
blocks.

**Should be squashed backward.** `MispredictPenalty` is set to 7 in
`0e0d39c89651` and changed to 11 here, four commits later, with the earlier
commit's message explicitly justifying 7 ("a seven-cycle branch mispredict
penalty"). A series submitted upstream should set 11 in the first place, with
this rationale. Likewise the `Rd_IMVIOp` split of the `Wr_IMVI` InstRW entry
by arity should have been written that way originally.

**Should be split.** The title admits two unrelated changes ("mispredict *and*
cross-cluster"). They share nothing.

**`???` committed into the source.** `AlphaSchedule.td`:

```
+  // ??? An itof result handed straight to an ftoi costs 8 cycles rather than
...
+  // Worth fixing, because section 3.7.1 of the Compiler Writer's Guide
+  // recommends this register path over bouncing a value through memory.
```

A `???` marker is not an LLVM convention; use `// TODO:`. The ten-line
lab-notebook comment is also *misplaced* — it sits immediately above
`def : ReadAdvance<Rd_ITest, 0>;`, which has nothing to do with it, so a
reader naturally attaches it to the wrong definition. And, per the finding
above, the premise is void: `MOVf2i` is expanded before scheduling, so
`Rd_FStData` is not carrying anything on that edge in the first place.

**No test.** Neither the `MispredictPenalty` change (which drives
if-conversion) nor the cross-cluster `ReadAdvance` has any test.

---

## a7dae61f34ba — [Alpha] Schedule after register allocation for the in-order cores

Small and correct in intent. Two concerns:

- **Interaction with epilogue CFI.** Commit `3037704af620` places CFI
  directives at exact positions in the epilogue and argues the state must be
  "correct at every instruction". The post-RA scheduler now runs on ev4/ev5
  and can reorder within a region; nothing here marks the CFI-bracketed
  epilogue instructions as scheduling boundaries. Worth verifying that
  `-mcpu=ev5` still produces async-precise epilogue CFI (add a `-mcpu=ev5` RUN
  line to `cfi.ll`).
- **The test change only weakens an existing test.** `prebwx-store-safe.ll`
  turns two ordered `CHECK`s into `CHECK-DAG`. There is no new test asserting
  that post-RA scheduling actually happens on ev4/ev5 and not on ev6.

---

## 3a1010615a8a — [Alpha] Emit lituse_tlsgd/lituse_tlsldm for dynamic TLS calls

Correct; use types 4/5 match the GNU toolchain, and the tests genuinely check
the relocation with `llvm-readobj -r` (the strongest tests in this chunk).

- `JSRtlsgd` and `JSRtlsldm` are byte-identical definitions differing only in
  the emitter's addend selection. Consider one instruction with a use-type
  operand, or at minimum note why two are needed.
- No test asserts `JSRd`/`JSRdl` still emit addend 3 after the emitter
  refactor. If an existing test covers it, fine; otherwise add a `RELOC` line.

---

## bb58f0db8237 — [Alpha] Emit IEEE software-completion trap qualifiers for -mieee

Encoding deltas verified: `+0x500` for `su`/`sv`, `+0x700` for `sui`/`svi`,
`+0x400` for `cvtst/s` (0x2AC → 0x6AC) — all correct, and `|=` is safe since
no base function field sets the trap bits.

**Known-bad design, fixed later.** Applying `-mieee` policy in
`AlphaInstPrinter::printInst` and `AlphaMCCodeEmitter::encodeInstruction` from
the subtarget feature bits means the MC layer silently rewrites hand-written
`addt` into `addt/su` whenever the feature is on, and an explicitly written
`addt/su` cannot round-trip. Commit `9ca05e38507d "[Alpha] Keep the -mieee
policy out of the MC layer"` (outside this chunk) undoes it. Squash that fix
backward rather than shipping the wrong design and correcting it later.

**Undocumented codegen change.** The commit also changes non-`-mieee`
`fpextend f32→f64` from `cvtst` to `cpys` (the new `FPEXTST` pseudo and the
`NotIEEE`/`HasIEEE` predicate split). This is a user-visible codegen change
for every non-`-mieee` build, and the commit title and message do not mention
it at all. Split it out, or describe it.

**Stale doc comment.** `AlphaInstrFormats.td`:

```
// 0 none, 1 arithmetic, 2 compare, 3 float-to-int, 4 int-to-float.
```

Class 5 (S-to-T convert) is added in this same commit and used by `CVTST`.

**Dead condition.** `getFPTrapSuffix` returns early when `!IEEE`, so
`case 5: return IEEE ? "s" : StringRef();` can never take the false branch.

**Disassembler.** Not updated: a word with the trap bits set (e.g. func 0x5A0)
has no decoder entry, so `addt/su` produced by this commit cannot be
round-tripped by `llvm-objdump`. Not mentioned in the message.

**Vacuous CHECKs in `ieee.ll`.**

- `; NONE: cvtqt` matches the prefix of `cvtqt/sui`, so the "no qualifier
  without -mieee" property is not tested for int-to-float.
- `; INEX: cmptlt/su` matches the prefix of `cmptlt/sui`, so the "compare
  takes su even with inexact" property is not tested either.
  Both need a trailing operand or `{{$}}`.
- The file has no `CHECK-LABEL` lines at all, so a directive can match in the
  wrong function.

---

## 1717696878a2 — [clang][Alpha] Add the -mieee and -mieee-with-inexact flags

Clean. Two gaps:

- The second paragraph of the message is entirely about the
  `ClaimAllArgs(OPT_mieee)` / `ClaimAllArgs(OPT_mno_ieee)` behaviour needed
  because "glibc passes both" — and `alpha-ieee.c` never tests that
  combination. Add `-mieee -mieee-with-inexact` and assert no
  `unsupported option` diagnostic.
- `-mno-ieee` is untested, and inside the with-inexact branch it is claimed
  and *ignored*, so `-mieee-with-inexact -mno-ieee` silently keeps IEEE on
  despite last-arg-wins. Either honour it or document why not.

---

## 37b6ead2f875 — [Alpha] Lower llvm.prefetch to R31/F31 loads

Encodings and the read/write × locality → ldl/ldq/lds/ldt mapping match the
21264 HRM. `PrefFrag`'s `getConstantOperandVal(2)`/`(3)` correctly index rw and
locality (operand 0 is the chain).

**The cache-type operand is ignored.** `llvm.prefetch`'s fourth argument
distinguishes an instruction-cache prefetch (0) from a data prefetch (1). The
patterns match `timm` for it and always emit a data prefetch, so
`llvm.prefetch(p, 0, 3, 0)` silently becomes a data prefetch. Either match
`cache == 1` explicitly and drop the icache case, or say why the distinction
does not apply.

**`FeaturePrefetchEvictNext` is unreachable.** No `ProcessorModel` enables it —
there is no `ev7`/`ev68` CPU in `Alpha.td`. The whole `PREFREN`/`PREFWEN` path
is reachable only via `-mattr=+prefetch-evict-next`, which is what the test
does. Add the processor, or drop the feature until there is one.

Relatedly, `generic` uses `Alpha21264Model` but does *not* get
`FeaturePrefetch`, so the default target schedules as an EV6 while dropping
prefetches. Probably intended (generic must run on EV4) but the asymmetry is
unremarked.

**Style.** `LowerOperation` returns `Op` itself for the has-prefetch case.
Returning `SDValue()` is the idiomatic "leave the node alone".

**Test.** Four prefetch calls, three `EV6-DAG` lines — the fourth
(write, locality 0 → second `lds $f31`) is unchecked, and there is no
assertion that exactly two `lds` are emitted. `EV4-NOT: $31` is very broad;
prefer `EV4-NOT: ldl $31` / `EV4-NOT: lds $f31`.

---

## 06708fb205ca — [Alpha] Use itoft/ftoit for integer/FP moves with the FIX extension

Encodings correct (`itoft` = 0x14.024, `ftoit` = 0x1C.070).

- No `AlphaSchedule.td` update: `ITOFT`/`FTOIT` get no `InstRW`, and (per the
  finding under `0e0d39c89651`) the `Wr_ItoF`/`Wr_FtoI` entries they should
  have inherited are attached to pseudos that no longer exist by scheduling
  time. On EV6 these have distinct latencies (3 vs 4) that the model claims to
  care about.
- Doing this in the custom inserter rather than as
  `let Predicates = [HasFIX] in def : Pat<(bitconvert ...), (ITOFT ...)>` keeps
  a pseudo alive through ISel for no benefit. Worth reconsidering, especially
  since the next commit has to grow the inserter into a four-way switch.

---

## b6fbd19b4eeb — [Alpha] Emit a .arch directive for the enabled extensions

**Correctness bug: CIX gets `.arch ev6`.**

```cpp
if (Features[Alpha::FeatureFIX] || Features[Alpha::FeatureCIX])
  Arch = "ev6";
```

GNU as's `ev6` accepts BWX, MAX and FIX but **not** CIX (`ctpop`, `ctlz`,
`cttz`); CIX requires `.arch ev67`. So the whole point of the commit — "so the
assembler accepts the extension instructions (bwx, mvi, cix/fix)" — fails for
exactly the CIX case. `-mcpu=ev67` output assembled by GNU as will be
rejected. Map CIX to `ev67`.

The parser side has the mirror-image bug:

```cpp
else if (Arch == "ev6" || Arch == "ev67" || Arch == "ev68")
  Feats = {"bwx", "cix", "fix", "mvi"};
```

`.arch ev6` should not enable `cix`. `arch-directive.ll` currently *pins* the
wrong behaviour (`-mattr=+cix` → `EV6: .arch ev6`).

**Untested.** The asm-parser half of the commit (the `.arch` directive
handler, including the error paths for a missing and an unknown architecture
name) has no test at all — `arch-directive.ll` only exercises the AsmPrinter.
Add an `llvm/test/MC/Alpha/arch-directive.s` that assembles a `ctpop` after
`.arch ev67` and rejects it after `.arch ev4`.

**Nit.** `.arch` only ever adds features; `.arch ev4` after `.arch ev6` does
not narrow the feature set, unlike GNU as. Worth a comment or a fix.

---

## 45dfd5063fae — [Alpha] Feed floating-point compares straight into fcmovne

Logic verified: `cmpteq/cmptlt/cmptle` leave 2.0/0.0 and unordered → 0.0,
which matches the O-prefixed conditions exactly; the swapped-operand forms for
`SETOGT`/`SETOGE` are right; and the `ne_select` test's apparent inversion is
correct (DAG canonicalization swaps the select arms, so `cmpteq` + `fcmovne`
picks the false arm on equality).

**Deletes coverage of a path it keeps.** `fp-select.ll` deletes `seli`:

```llvm
define double @seli(i64 %c, double %t, double %f) { ... }
```

That was the only test of the `Pat<(f32/f64 (select GPRC:$c, ...))>` fallback
— which this commit explicitly retains ("Otherwise move the 0/1 condition into
an FP register…"). Keep it.

`selcmp` is also deleted and replaced by an equivalent `fsel`; that rename is
pure churn.

**Coverage.** Ten `FSelPat` instantiations × 4 (svt × cvt) = 40 new patterns;
the tests exercise two of them (`SETOLT` f64/f64, `SETEQ`-ish f64/f64). None
of the f32 comparison, f32 select, or `SETOGT`/`SETOGE` operand-swap forms is
tested — and the operand swap is the one most likely to be backwards.

`SETUNE`, `SETONE`, `SETUEQ`, `SETO`, `SETUO` are not covered by any pattern
and fall back to the slow path; the message does not say so.

---

## ee52db3535d2 — [Alpha] Support 4-byte atomics with ldl_l/stl_c

**Should be split.** Three independent changes:
1. `ATOMIC_CMPXCHG_I32` / `ldl_l`+`stl_c` (the title),
2. `getExtendForAtomicOps` + sign-extending every narrow atomic result — a
   *miscompile fix* ("a negative narrow atomic result miscompared against a
   constant") that deserves its own commit and its own regression test,
3. the BWX-free sign-extension expansion.

Only (1) is in the title; (2) is the most important of the three.

**Minor inefficiency.** In `emitAtomicCmpXchg` the `ADDL $31, Cmp` that
sign-extends the expected value is built into `LoopBB`, so it is recomputed on
every ll/sc retry. It is loop-invariant; emit it in the entry block. (The test
even pins it inside the loop region.)

**Question on the min/max path.** The comment added to
`atomic-rmw-minmax.ll` argues that an unsigned sub-word compare on
sign-extended operands is safe because sign extension preserves unsigned byte
ordering. That reasoning is correct — but only if *both* operands are
sign-extended. Please confirm the incoming `%v` operand is guaranteed
sign-extended in the non-BWX path (the test's `sll ..., 56,` suggests it is,
but the invariant lives only in a test comment, not in the code). It belongs
as an assertion or a comment in `emitSubwordAtomicRMW`.

**`atomic-subword-nobwx.ll`'s `-NOT` lines cannot fail.** `SEXTB`/`SEXTW` are
`let Predicates = [HasBWX]`, so a non-BWX subtarget can never emit them. Every
`NOBWX-NOT: sextb` / `NOBWX-NOT: sextw` line is vacuous by construction. Three
of the five functions (`rmw_i8`, `cas_i16`, `umin_i8`) have *only* those
vacuous lines plus `ldq_l`/`stq_c`, so the actual shift-pair expansion is
unchecked there. Replace with positive `sll`/`sra` checks like `load_i8` uses.

---

## 95ca3bea6cc1 — [Alpha] Materialize the float constants that need no load

Correct: `$f31` reads +0.0, `cmpteq $f31,$f31` yields exactly +2.0, and 0.0/2.0
have identical register bits in S and T form so the f32 patterns are sound.

**Test file is malformed.**

```llvm
define double @zero() { ret double 0.0 }
define float  @zerof() { ret float 0.0 }
define double @two() { ret double 2.0 }
define float  @twof() { ret float 2.0 }
; CHECK-LABEL: zero:
...
; CHECK-LABEL: negzero:
; CHECK: cpysn $f31, $f31, $f0
define double @negzero() { ret double -0.0 }
```

- `zerof` and `twof` have **no CHECK lines** — the f32 half of the feature is
  untested despite being half the `foreach vt = [f32, f64]`.
- All definitions but one precede all checks, and `negzero` is defined after
  its checks. Every other test in this series interleaves per-function.
- No `CHECK-NOT` for a constant-pool reference (`.rodata`, `ldt ... (gp)`), so
  the test does not verify the pool load is actually gone — which is the whole
  point.

---

## d2579856cf99 — [Alpha] Use itofs/ftois for f32/i32 bit casts with the FIX extension

Encodings correct (`itofs` = 0x14.004, `ftois` = 0x1C.078); the non-FIX
`stl`/`lds` and `sts`/`ldl` fallbacks are the right memory-format conversions.

- Consider combining with `06708fb205ca`: this commit rewrites that commit's
  two-line `unsigned Opc = ... ? ITOFT : FTOIT` into a four-way switch, i.e.
  the earlier commit was written knowing this one was coming. One commit
  "Use the FIX integer/FP moves" would be cleaner.
- No `AlphaSchedule.td` entries for `ITOFS`/`FTOIS`/`MOVi2f_S`/`MOVf2i_S`.
- `ReplaceNodeResults` handles exactly one node kind and silently does nothing
  otherwise. That is conventional, but since this is the target's *first*
  `ReplaceNodeResults`, a one-line comment on the contract would help.

---

## df6f7d190ce3 — [Alpha] Lower any byte-granular AND mask to a single zapnot

Correct and well-scoped; `bytemask`/`zapnotmask` are right, including the
negative-constant case (`-256` → keep `0xFE`).

- The degenerate masks are accepted: all-zero → `zapnot $x, 0` and all-ones →
  `zapnot $x, 255`. Both are folded away by DAGCombine first, so this is
  harmless, but an early `return M != 0 && M != ~0ULL;` would document intent.
- Test does not re-cover the three masks being generalized (`0xff`, `0xffff`,
  `0xffffffff`). If existing zero-extension tests cover them, add a comment;
  otherwise this commit silently drops the only coverage of the code it
  replaces.

---

## 327dd5f881db — [Alpha] Add the longword scaled add/subtract instructions

Encodings correct (0x10.02 / 0x12 / 0x0B / 0x1B), and the
`sext_inreg(add(shl(a,N), b), i32)` pattern is sound because the low 32 bits of
the 64-bit shift-and-add are congruent with the 32-bit computation.

**Deletes existing coverage.** `scaled-add.ll` removes `gep8`
(`s8addq` for array indexing — the canonical use, and the only test of
scaled-add for a GEP), removes `s8s` (`s8subq`, leaving the quadword subtract
form entirely untested), and deletes the explanatory header. A commit that
*adds* the longword forms should not remove the quadword tests; keep them and
append.

**No `AlphaSchedule.td` entries** for `S4ADDL`/`S8ADDL`/`S4SUBL`/`S8SUBL`,
though the quadword forms are in `Wr_IALU`.

---

## 167a60b4f245 — [Alpha] Extract a byte/word/longword at a constant offset with ext

Correct. I checked the boundary positions: the ARM defines `EXTxL` as
`(Rav SRL byte_loc*8) AND mask`, so the pattern is exact even at positions
5–7 where the field runs past the end of the quadword.

- `byteshift` accepts a shift of 0, so `and x, 0xff` with no shift now
  competes with `zapnot $x, 1` from the previous commit — both one
  instruction, but the `AddedComplexity = 1` on both makes the winner
  arbitrary. Worth pinning in a test or excluding `S == 0`.
- No `AlphaSchedule.td` entries for `EXTBLi`/`EXTWLi`/`EXTLLi`.
- Test covers three of the three patterns — good — but no boundary position
  (e.g. `lshr 56` + `& 0xffffffff` → `extll $x, 7`), which is where a
  shift-vs-extract mismatch would show up.

---

## 9d80d1057b47 — [Alpha] Select ext/ins/msk for byte ops at a variable position

The correctness argument in the commit message is right, and I verified it
holds even for the non-obvious cases: when `n << 3` wraps modulo 2^64 to a
value below 64, that value equals `8 * (n mod 8)`, so the ext/ins/msk position
agrees; when it is ≥ 64 the IR shift is poison, so any lowering is legal.

**Coverage.** Nine new patterns, three tested (`extbl`, `inswl`, `mskbl`).
`extwl`, `extll`, `insbl`, `insll`, `mskwl`, `mskll` are untested — and the
`msk` patterns are the most intricate of the nine (`and x, (not (shl mask,
(shl n, 3))))`), so at minimum add `mskll`.

No `AlphaSchedule.td` concern here (the register forms are already mapped).

---

## c4997c8611ed — [Alpha] Fold a sign-extended 32-bit operation into addl/subl/mull

Encodings correct (0x10.00 / 0x10.09 / 0x13.00), and the patterns are sound
(low 32 bits of a 64-bit add/sub/mul are congruent with the 32-bit result).

- **No `AlphaSchedule.td` entries** for `SUBL`, `MULL`, `ADDLi`, `SUBLi`,
  `MULLi`. `MULL` is the serious one: it falls out of `Wr_IMul` entirely, so
  a 32-bit multiply schedules as if it were free on every model.
- `ADDL` previously had an empty pattern and was used explicitly by
  `Pat<(sext_inreg GPRC:$x, i32), (ADDL GPRC:$x, R31)>`, by the `sextl`
  InstAlias, and by `emitAtomicCmpXchg`. Giving it a pattern creates two
  routes to the same instruction; worth a note that the standalone
  `sext_inreg` pattern is still needed.
- Tests cover `addl`, `subl`, `mull`, `addl`-with-literal. `SUBLi` and `MULLi`
  are untested.

---

## 4b5f9b8bc17f — [Alpha] Strength-reduce multiply by a constant

**Unexplained magic condition.**

```cpp
unsigned TZeros = MulC == 2 ? 0 : MulC.countr_zero();
```

Nothing in the code or the commit message explains the `MulC == 2` case, and
it is a no-op: with it, `MulC` stays 2 and `(2-1).isPowerOf2()` is true;
without it, `MulC` becomes 1 and `(1+1).isPowerOf2()` is true. Either delete
it or comment why it is needed.

Also `Imm == 0` reaches `countr_zero() == 64` and `lshrInPlace(64)`, returning
true for `x * 0`; harmless (DAGCombine folds it earlier) but the function
should not depend on that.

**Mixed concern.** The `Pat<(sub (i64 0), GPRC:$x), (SUBQ R31, GPRC:$x)>`
negation fix is unrelated to multiply strength reduction; the message
acknowledges this with "Also match a negation to…". Small enough to leave, but
it is a general codegen improvement that deserves its own line of test
coverage independent of `muln5` (which it does get, in `negate`).

**Test.** `mul-const.ll` is thorough for the constants it covers, but runs
only `-mcpu=ev6` even though `decomposeMulByConstant` is subtarget-independent
and matters far more on EV4 (mulq = 23 cycles). No shifted-constant case
(`x * 6`, `x * 24`) exercising the `TZeros` path, which is the only
non-trivial arithmetic in the function.

---

## 9b9e5711781b — [Alpha] Materialize all 32-bit constants inline

The best commit in the chunk: real bug ("Cannot select" for high half 0x8000),
clean refactor (hoisting the `add32` lambda into `buildConstant32`), stated
side effect, and five new tests that pin exact immediates including both
boundary cases.

One nit: the comment on the `int32_min` test says "INT32_MIN = 0x80000000
sign-extends from a single ldah", but the test body is `ret i64 -2147483648`,
i.e. `0xFFFFFFFF80000000`. Reword to avoid conflating the 32-bit pattern with
the 64-bit value.

---

## 44a29238297d — [Alpha] Store zero from the zero register

Patterns are correct; `AddedComplexity = 1` is needed and present.

**Commit message is garbled.**

> Match the store of zero directly to a store of $31 (or $f31 for the
> byte/word forms via the zero GPR)

`$f31` has no role here — all four patterns use `R31`, and byte/word stores
take a GPR. Delete the parenthetical.

**Gaps.**

- The `truncstorei8`/`truncstorei16` patterns are inside
  `Predicates = [HasBWX]`, so a pre-BWX byte/word store of zero still runs the
  full `ldq_u`/`mskbl`/`insbl`/`stq_u` sequence with an `insbl` of a
  materialized zero. Either handle it or say it is out of scope. The test runs
  only `-mcpu=ev6`, so the non-BWX path is not even observed.
- No `stt $f31` / `sts $f31` for a store of `0.0`, which is the natural
  companion given `95ca3bea6cc1` just made `0.0` materialize as
  `cpys $f31, $f31`.

---

## Cross-cutting recommendations

1. **Squash the later fixes backward**: `4fa520dcb620` ("Complete the
   scheduling models") into `0e0d39c89651` and into each instruction-adding
   commit; `9ca05e38507d` ("Keep the -mieee policy out of the MC layer") into
   `bb58f0db8237`; `01b5aa0ae6f1`'s `MispredictPenalty = 11` into
   `0e0d39c89651`.
2. **Delete all commit-message meta-commentary about commit granularity**
   (`1af22d79e410` ¶3, `0e0d39c89651` ¶5, and the "Keeping the three in one
   commit is deliberate" pattern generally). Delete the trailing orphan
   paragraphs in `cd617a876f3a` and `0a0690123ab2`.
3. **Add an "update AlphaSchedule.td" step** to every commit that adds an
   instruction; `CompleteModel = 0` is currently hiding a growing hole.
4. **Fix or remove the two silent-wrong-code paths** before submission:
   the unvalidated `ret`/`jmp` parsing in `0a0690123ab2` and the `.arch ev6`
   for CIX in `b6fbd19b4eeb`.
5. **Strip series-history narration from test files** (`alpha-mcpu.c`) and
   replace test-header paragraphs that restate the commit message with a
   one-line statement of the invariant under test.
