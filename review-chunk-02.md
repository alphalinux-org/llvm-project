# Review: commits 12..40 (backend skeleton + first ISel wave)

Range: `5eb60e54dd1a` .. `9ace8008e4c4` on `alpha-triple-0bf3638`.

## Summary — highest priority

1. **`7ce4f183a39f` ships five wrong instruction encodings.** `CMPEQ`, `CMPLT`,
   `CMPLE`, `CMPULT`, `CMPULE` are all defined as `CMP_rr<0x10, ...>` — the same
   function code, and not the right one for any of them (correct: 0x2d, 0x4d,
   0x6d, 0x1d, 0x3d). Silently corrected 90 commits later in `5e5e91b7aafb`,
   whose message even claims "No functional change". Fix belongs in
   `7ce4f183a39f`.
2. **`eabc77df741a`'s `selcmp` test passes for the wrong reason.** It binds
   `[[C]]` from `cmptlt` and re-uses it in `fcmovne [[C]], ...`, which reads as
   "the compare result feeds fcmovne directly". It does not — at that commit the
   condition round-trips through memory twice (`MOVf2i`/`srl`/`MOVi2f`); the
   check only matches because regalloc happened to reuse the same `$f`
   register. `45dfd5063fae` later rewrote the test with explicit
   `CHECK-NOT: stt` / `CHECK-NOT: ldq` and stated exactly that.
3. **`49fd1690fdb7`'s commit message contradicts its own diff.** The message
   says the change "is keyed on `isInt<32>`" and that `[0x7fff8000,0x7fffffff]`
   constants "are not sent to the pool, and there is no pattern for them either.
   Handled in a later commit." The code is keyed on `!isInt<16>(Hi)` and its
   inline comment says it *does* handle "those whose high half is 0x8000".
4. **`1b9fe58dbb1e` lowers `fpext f32->f64` to `cvtst`**, but an S_floating
   value already sits in the register in T format, so the extension is free.
   GCC emits `fmov`. The series itself later adds `FPEXTST` (a `cpys`) at
   `AlphaInstrInfo.td:688`, confirming the pessimization.
5. **`7a1b1dc184e9` has no test**, and creates a fresh 8-byte stack object per
   `MOVi2f`/`MOVf2i` instance, so frame size grows linearly with the number of
   bitcasts/conversions in a function.

---

## `5eb60e54dd1a` — Add experimental backend skeleton

**Correctness**

- `AlphaCallingConv.td:15-22` — the comment claims "(register slots are
  shared)" but `CCAssignToReg` with two independent lists does *not* share
  slots; `f(double,long)` would put the `long` in `$16` instead of `$17`. Dead
  code at this commit (argument lowering `report_fatal_error`s), and the very
  next commit replaces it with `CCAssignToRegWithShadow`. Either land the
  correct form here or drop `CC_Alpha`/`RetCC_Alpha` from the skeleton entirely.
- `AlphaRegisterInfo.cpp` `getReservedRegs` reserves `Alpha::R15`
  unconditionally, yet `R15` is also in `CSR_Alpha`. When `hasFP()` is false
  (the common case) this permanently loses a callee-saved register.
- `AlphaAsmPrinter.cpp` `lowerOperand`'s `default: report_fatal_error(...)`
  will fire on `MO_CFIIndex`, and `AlphaMCAsmInfo.cpp` sets
  `ExceptionsType = ExceptionHandling::DwarfCFI` and
  `SupportsDebugInformation = true`. `emitInstruction` should skip
  `MI->isCFIInstruction()` / `isDebugInstr()`.
- `AlphaInstrInfo.td:47-50` — `def NOP : AlphaInst<...> { let Opcode = 0x11; }`
  leaves the rest of `Inst` unset; the comment says "NOP is the canonical
  `bis $31, $31, $31`" but nothing sets Ra/Rb/Rc=31. Fixed only in
  `5e5e91b7aafb`.

**LLM-tells / prose**

- `AlphaInstrFormats.td:22-24`: "A placeholder for fields an instruction does
  not set, so the disassembler decoder tables can be generated once real
  encodings exist." That is not what `SoftFail` is (it marks bits allowed to
  differ during decode), and there is no disassembler in this commit. Drop the
  field and the comment.
- `AlphaRegisterInfo.td:88-91` — the FPRC comment ("with f32 first the class
  claimed to be 32 bits wide and a double in it did not match its own class")
  is a debugging war story, not a code comment. The same story is repeated
  verbatim in the commit message. Keep at most one, one sentence.
- The commit message's bulleted `- TableGen: ... - MCTargetDesc ... - CodeGen:`
  structure is not LLVM style; prose paragraphs are conventional.
- `test/CodeGen/Alpha/global.ll:3-5` — "The Alpha backend is still a skeleton:
  only the target registration, data layout and global/data emission work.
  Instruction selection is not implemented yet, so this test only exercises the
  data path." A comment guaranteed to go stale within three commits.
- `global.ll:14` — "A 64-bit pointer is emitted little-endian as a `.quad`."
  `.quad 0` has no observable endianness; the comment restates the CHECK.

**Tests**

- `global.ll` hardcodes `target datalayout = "e-m:e-p:64:64-..."`, which
  *overrides* the target's computed layout — so the commit's claim that "the
  data layout ... work[s]" is not actually tested. Drop the line and let the
  triple supply it.
- Commit message says features are "(bwx/max/fix/cix)" but the TableGen feature
  is named `mvi`, not `max`.

## `18e57be6d1aa` — Lower formal arguments and returns

- The shared-slot ABI (`CCAssignToRegWithShadow`) is the substantive change here
  and is **not tested**. `ret.ll` only has all-integer or all-FP signatures,
  which pass identically with the broken independent-list version. The mixed
  test (`call-arg-slots.ll` `@mixed`, `addt $f17, $f19`) lands 12 commits later
  in `5c1f00228c19`; move it here.
- `ret.ll:3-5` restates the commit message as a file header comment; delete.

## `dba8e92dce87` — Select register-register integer arithmetic

**Structure** — the commit adds *seven* instruction-format classes (`OForm`,
`OFormL`, `FForm`, `MForm`, `MFormD`, `BForm`, `MbrForm`) but only uses
`OForm`. `MForm` in particular is never used by this chunk at all (`LDQ`/`STQ`
are plain `AlphaInst` with a bare `let Opcode`), and `5e5e91b7aafb`'s message
later claims to "Add the OForm/OFormL/FForm/MForm/MFormD/BForm/MbrForm format
classes" — i.e. that commit thinks it introduces them. Add each format in the
commit that first uses it.

- `arith.ll:3` — "Register-register integer arithmetic and logical operations on
  i64." Summary comment restating the filename/commit; delete.
- Encodings checked and correct: addq 0x10/0x20, subq 0x10/0x29, mulq 0x13/0x20,
  and 0x11/0x00, xor 0x11/0x40, bis 0x11/0x20.

## `8094bb291ca3` — Materialize small signed constants with lda

- `imm.ll:3-5` — "Only immediates that fit in the 16-bit signed displacement are
  handled for now; wider constants are not yet supported." Stale two commits
  later (`46b4919f85ef`); the commit message already says this.

## `52fd03b365bc` — Select aligned loads and stores

- Operand order is inconsistent: `memri` is `(ops GPRC, s16imm)` (base, disp)
  but `LDA`/`LDAH` are `(ins s16imm:$disp, GPRC:$Rb)` (disp, base), and
  `MFormD` binds `Rc/Rb/disp`. Worth making uniform before the encoder lands.
- `LDQ`/`LDT`/`LDS`/`STQ`/`STT`/`STS` use bare `AlphaInst` + `let Opcode` even
  though `MForm` was added in the previous commit specifically for them.
- **Untested**: `STS` (f32 store) is added but `load-store.ll` has no `sts`
  case. Also untested: a non-zero displacement on a store, and the >16-bit
  offset fallback path in `SelectADDRri`.
- `SelectADDRri` shadows `FIN` in two nested scopes; harmless but noisy.

## `6fb6f03e71a7` — Select register-register shifts

- `shift-mask.ll` does not test anything this commit adds. Removal of the
  redundant `and` on a shift amount is done unconditionally by generic
  `SimplifyDemandedBits` (shifts >= bitwidth are poison in IR), with no target
  hook involved — grep confirms Alpha sets none. The test cannot regress from an
  Alpha change; either drop it or state in the comment that it guards the
  generic behaviour.
- The two tests added by one commit use opposite CHECK conventions:
  `shift.ll` puts CHECK lines *before* `define`, `shift-mask.ll` puts them
  *inside* the function. Pick one.
- `shift.ll:3` — header comment restating the commit message.

## `7ce4f183a39f` — Select integer comparisons (setcc)

- **BUG (encoding).** `AlphaInstrInfo.td`:
  ```
  def CMPEQ  : CMP_rr<0x10, "cmpeq">;
  def CMPLT  : CMP_rr<0x10, "cmplt">;
  def CMPLE  : CMP_rr<0x10, "cmple">;
  def CMPULT : CMP_rr<0x10, "cmpult">;
  def CMPULE : CMP_rr<0x10, "cmpule">;
  ```
  All five share function code 0x10; none is correct. Should be 0x2d, 0x4d,
  0x6d, 0x1d, 0x3d. Squash the fix out of `5e5e91b7aafb` into this commit —
  `5e5e91b7aafb`'s "No functional change to assembly output" is then also true
  again for that commit.
- `setcc.ll` tests 7 of the 10 conditions added: `SETGE`, `SETUGT`, `SETUGE`
  have patterns but no test. Add sge/ugt/uge.
- `; CHECK: xor $0,` in `@ne` is loose enough to match almost any `xor`.
- The `SETNE` pattern materialises 1 with `(LDA 1, R31)`; `XORi` exists two
  commits later (`87b5376219bb`) but the pattern is only updated in
  `0ad758bec362`, so `icmp ne` costs an extra instruction across ~50 commits.

## `f7a2bf2d6b85` — Select i64 select via conditional move

- `CMOVNE` encoding 0x11/0x26 is correct; the tie-constraint form is right.
- `select.ll` `@seltrunc` checks only `; CHECK: cmovne`. The interesting part of
  that test case is that `trunc i64 %c to i1` must produce an explicit `and`
  with 1 before `cmovne` (which tests the whole 64-bit register, not bit 0).
  The CHECK cannot distinguish correct from miscompiled output — add the `and`.

## `5363bd96beda` — Select branches

- `BEQ` is defined with an empty pattern and is used by nothing in this commit;
  the message claims "conditional branches BEQ and BNE, with patterns for br and
  brcond" but only BNE has a pattern. Either add the inverted-condition pattern
  or defer `BEQ`.
- `BR` is added but no CHECK line anywhere asserts `br $31, ...` — `branch.ll`
  only checks `bne` and `ret` in both functions.
- `branch.ll` checks are near-vacuous: `@diamond` asserts `bne $0,` and `ret`,
  which does not verify the branch target, the fallthrough, or the compare.
- `analyzeBranch`/`insertBranch`/`removeBranch` are not implemented, so
  BranchFolding and MachineBlockPlacement are inert. Worth a note in the message.
- Comment in `AlphaISelLowering.cpp` says "there are no jump tables yet" next to
  the `BR_JT` Expand — that is what the code says; drop the restatement, keep
  only the `BRCOND`-only rationale.
- Encodings br 0x30, beq 0x39, bne 0x3d are correct.

## `53d571d0bf83` — Implement frame lowering

- **`hasFP()` is honoured by `getFrameRegister` but the prologue never
  establishes `$15`.** `hasFPImpl` returns `hasVarSizedObjects()`, so a function
  with a dynamic alloca will have `eliminateFrameIndex` rewrite frame indices
  against `$15`, which holds garbage. The FP setup only appears much later
  (current `AlphaFrameLowering.cpp:82`). Either make `hasFPImpl` return `false`
  here, or add the setup in this commit.
- No CFI is emitted, while `AlphaMCAsmInfo` advertises `DwarfCFI`. Unwinding
  through an Alpha frame is broken from here until CFI lands; say so in the
  message.
- `storeRegToStackSlot`/`loadRegFromStackSlot` pick `STT`/`LDT` for anything
  that is not GPRC, including f32. That is correct (an S value lives in the
  register in T format so `stt`/`ldt` round-trips exactly) but it is
  non-obvious — worth a one-line WHY comment, which is exactly the kind of
  comment missing while several WHAT comments are present.
- `frame.ll` has no test for a spill/reload (`storeRegToStackSlot` is untested),
  and none for the FP-register spill path.

## `46b4919f85ef` — Materialize wider constants with ldah/lda

- `imm32.ll` `@shifted` enshrines a redundant instruction:
  ```
  ; CHECK:       ldah $0, 1($31)
  ; CHECK-NEXT:  lda $0, 0($0)
  ```
  `Lo == 0` should emit `ldah` alone. Fixed later (HEAD's `imm32.ll` says
  "65536 has a zero low half, so the lda is omitted"); do it here rather than
  writing a test that asserts the worse output.
- Untested boundary cases the code implicitly decides: `0x40000000`,
  `INT32_MIN`, `0x7fff8000`. HEAD's version of this file adds them.

## `5020677fd0bc` — Select IEEE floating-point arithmetic

- Encodings 0x080/0x0a0/0x081/0x0a1/0x082/0x0a2/0x083/0x0a3 are correct.
- Eight instructions added, five tested: `SUBS`, `MULT`, `DIVS` have no test.

## `8930ce0fc764` — Select floating-point sign operations

- **`FABSS`/`FABST`, `FNEGS`/`FNEGT`, `FCPYSS`/`FCPYST` are not marked
  `isCodeGenOnly = 1`.** Each pair has an identical operand list and identical
  `AsmString`; they only differ in the pattern's value type. This is exactly the
  ambiguity that breaks the AsmMatcher/disassembler once those land — and the
  series does add `let isCodeGenOnly = 1 in` to all six later
  (`AlphaInstrInfo.td:603,604,612,613,630,631`). Add it here.
- These use bare `AlphaInst` with only `let Opcode = 0x17` — the 11-bit function
  field (cpys 0x020, cpysn 0x021) is never set. Same class of deferred-encoding
  debt as the CMP bug; here it is at least not *wrong*, merely absent.
- `fp-sign.ll` has no `fneg float` and no `copysign f32` case, though `FNEGS`
  and `FCPYSS` are added.

## `c74d01991ef8` — Zero-extend narrow values with zapnot

- `zapnot` 0x12/0x31 correct; masks 1/3/15 correct.
- The commit's rationale (avoids materialising 0xffffffff, which cannot yet be
  built) is genuinely useful and correctly placed.
- Untested: any `and` with a mask that is *not* one of the three, e.g.
  `and i64 %x, 65280` — which at this point cannot be selected at all.

## `87b5376219bb` — Select immediate-form operate instructions

- `SUBQi`'s pattern `(sub i64:$Ra, immUExt8:$lit)` is effectively dead:
  DAGCombiner canonicalises `sub x, C` into `add x, -C`, so `x - 5` will
  materialise `-5` with `lda` and use `ADDQ` rather than `subq $16, 5, $0`.
  Either add a `(add x, negimm) -> SUBQi` pattern or drop the claim. Nothing in
  `alu-imm.ll` tests `subq` with an immediate, so the gap is invisible.
- `MULQi` is likewise added with no test.
- Timestamps are out of order relative to the neighbour: this commit is
  11:22:59, the next one (`216703c46f12`) is 11:21:00. Cosmetic, but it shows
  the reordering.

## `216703c46f12` — Sign-extend narrow integers

- `sext.ll` `@add32` enshrines two instructions where one suffices:
  ```
  ; CHECK:       addq $16, $17, $0
  ; CHECK:       addl $0, $31, $0
  ```
  `ADDL` computes add-and-sign-extend in one instruction; the pattern
  `(sext_inreg (add i64:$Ra, i64:$Rb), i32)` is a one-liner and is what HEAD
  has (`AlphaInstrInfo.td:502`, and HEAD's `sext.ll` now says "a single addl").
  Add it here rather than shipping and then fixing a test that asserts the worse
  sequence.
- `ADDL` 0x10/0x00 is correct.

## `0e6417e3359a` — Select 32-bit loads and stores

- `ldl` 0x28 / `stl` 0x2c correct. Patterns look right.
- No issues found beyond the general `AlphaInst`-instead-of-`MForm` point.

## `1b9fe58dbb1e` — Select f32/f64 precision conversions

- **`fpext f32 -> f64` should not emit an instruction.** `lds` already leaves
  the S value in the register in T_floating format, so the extension is a
  register move at worst. GCC emits `fmov` for `extendsfdf2` and only reaches
  for `cvtsts` under `-mieee` software completion. This series ends up agreeing:
  `AlphaInstrInfo.td:688` defines `FPEXTST : FMov<[(set f64:$Rc, (fpextend
  f32:$Rb))]>`. Emitting `cvtst` costs an instruction on every float-to-double
  promotion.
- `CVTST`/`CVTTS` are bare `AlphaInst` with only `Opcode = 0x16`; the function
  fields (0x2ac / 0x0ac) are absent.
- `fp-convert.ll` names a function `@trunc`, which shadows the `trunc` IR
  keyword in the reader's eye; `fptrunc`/`fpround` would be clearer.

## `7a1b1dc184e9` — Move bits between the integer and floating registers

- **No test.** The commit adds `MOVi2f`/`MOVf2i`, a custom inserter, and the
  `bitconvert` patterns, and ships zero test coverage. The `@bitcast` function
  in the next commit's `int-fp.ll` is the test for this commit; move it here.
- **A new stack object per instance.** `CreateStackObject(8, Align(8), false)`
  runs once per `MOVi2f`/`MOVf2i` MachineInstr, so a function with N bitcasts
  gets an N*8-byte frame. Cache one slot in `AlphaMachineFunctionInfo` (as
  other targets do) — this also matters because `adjustStack` hard-errors above
  32 KiB.
- No `MachineMemOperand` is attached to the generated `STQ`/`LDT`, so the
  scheduler and alias analysis treat them as unknown memory.
- The pseudos are not marked `mayLoad`/`mayStore` even though they expand into
  a store and a load.

## `24278df5bb0a` — Convert between integer and floating values

- `(i64 (fp_to_sint f32:$x)) -> (MOVf2i (CVTTQ (CVTST f32:$x)))` inserts a
  `cvtst` that is a no-op for the reason above; `cvttq` can read the register
  directly.
- Four patterns added, three tested: the `fp_to_sint f32` path is untested.
- `int-fp.ll` `@sitofp_f32` checks only `cvtqs $f0, $f0` and skips the stack
  bounce that the header comment advertises.
- `CVTQT`/`CVTQS`/`CVTTQ` again carry no function code (0x0be / 0x0bc / 0x0af).

## `0e2536386bb5` — Select complement logical operations

- `bic` 0x08, `ornot` 0x28, `eqv` 0x48 all correct; the `not` -> `ornot $31, x`
  pattern is right.
- Untested: the commuted `and (not b), a` form, and the immediate forms
  (`bic $Ra, lit, $Rc`) are not provided, so `and i64 %x, -256` still needs a
  materialised constant.

## `52acfca806c9` — Select byte/word memory access with BWX

- Encodings ldbu 0x0a, ldwu 0x0c, stw 0x0d, stb 0x0e are correct, and the
  `Predicates = [HasBWX]` nesting is correct.
- `bwx-mem.ll` runs with `-mcpu=ev6`, which has BWX and therefore `sextb`
  (0x1c/0x00) and `sextw` (0x1c/0x01). The test asserts a three-instruction
  `ldbu; sll 56; sra 56` sequence for `@sext8` where `ldbu; sextb` would do.
  Another test asserting output the series later improves.
- The `setLoadExtAction(SEXTLOAD, ...)` call is unconditional but the
  instructions it depends on are BWX-gated; without BWX a `load i8` has no
  pattern at all and will hit "Cannot select". The message acknowledges this,
  but there is no test (even an XFAIL/`not llc`) pinning the default-CPU
  behaviour, so the regression surface is invisible.

## `ef58a1225d42` — Materialize global addresses via the GOT

- **`LDGP` is `isPseudo = 1` yet emits real text**, and `AlphaInst` fixes
  `Size = 4` while `ldgp` is a gas macro expanding to two instructions (8
  bytes). `getInstSizeInBytes` will under-report by half. Same problem, worse,
  in the next commit (see `JSR`).
- `LDQl`'s `Uses = [R29]` is right, but nothing in this commit ties the
  `usesGP()` flag to the reserved-ness of `$29`; fine today because `$29` is
  unconditionally reserved, but that also means the *unused* `$29` is wasted in
  every leaf function.
- `global-address.ll` uses `{{.*}}` before `!literal`, which is permissive
  enough to hide a wrong relocation suffix ordering; `{{[[:space:]]*}}` or an
  exact match would be tighter.
- `AlphaMachineFunctionInfo` has an unused `(const Function &, const
  TargetSubtargetInfo *)` constructor body — fine, but it needs `clone()` if
  MIR round-tripping is ever wanted; worth a note.

## `5c1f00228c19` — Lower direct calls

- **`JSR`'s `AsmString` is two instructions**:
  `"jsr $$26, ($$27)\n\tldgp $$29, 0($$26)"`. `ldgp` itself is a macro that
  expands to `ldah`+`lda`, so this single MachineInstr with `Size = 4` occupies
  12 bytes. Any branch-relaxation, `getInstSizeInBytes`, or inline-asm size
  computation will be wrong. Split into a `JSR` + a separate post-call `LDGP`
  (or make the size explicit).
- The `CSR_Alpha` / `CSR_Alpha_Call` split (R26 callee-saved but not
  call-preserved) is correct and well explained — good commit message.
- `eliminateCallFramePseudoInstr` unconditionally erases without adjusting `$sp`
  by `getCallFrameSize()`. Safe only because stack args `report_fatal_error`;
  the `NumBytes = CCInfo.getStackSize()` value computed just above is silently
  discarded. Worth an `assert(NumBytes == 0)`.
- **`call-arg-slots.ll` belongs in `18e57be6d1aa`** — it tests the
  `CCAssignToRegWithShadow` behaviour introduced there. The `@caller` half
  legitimately belongs here.
- `call-arg-slots.ll` `@mixed` tests the register assignment only indirectly,
  via `stq $16,` / `stq $18,` from the sitofp stack bounce. That coupling means
  the test breaks when the `MOVi2f` slot handling changes. The
  `addt $f17, $f19` check is the good one.

## `4838f88319d0` — Materialize constant pools GP-relative

- Two changes in one commit: GP-relative `ConstantPool` lowering, and the
  `(f64 (extloadf32 addr)) -> (CVTST (LDS addr))` pattern. The second is a
  separate concern, and is also a pessimization for the same reason as
  `1b9fe58dbb1e`: `lds` already yields the T-format value, so the `cvtst` is
  redundant. `fp-constant.ll` `@add1` asserts `lds; cvtst` as expected output.
- `fp-constant.ll` hardcodes `.LCPI0_0` in `@add1` but uses `{{.*}}` in
  `@addpi`, so the second function does not verify that the two halves reference
  the *same* constant-pool symbol — a wrong-symbol bug would pass.

## `49fd1690fdb7` — Materialize constants wider than 32 bits from the pool

- **The commit message describes code that is not in the diff** (see Summary
  item 3). Rewrite it to match: the predicate is `!isInt<16>(Hi)`, which covers
  both >32-bit values *and* the 0x8000-high-half range.
- The `LDQ` machine node is built via `getMachineNode` with no chain and no
  `MachineMemOperand`, unlike every other `LDQ` in the backend (which gets both
  from the `load` pattern). Use `MachinePointerInfo::getConstantPool` at
  minimum.
- No test for the boundary case the code's own comment calls out
  (`0x7fff8000`..`0x7fffffff`); `imm64.ll` has a single function.

## `dce08602d2ed` — Select floating-point comparisons

- Encodings cmpteq 0x0a5 / cmptlt 0x0a6 / cmptle 0x0a7 correct; the
  2.0-or-0.0 → `srl 62` idiom is correct, and sharing the T-format instructions
  between f32 and f64 is correct.
- **Commit message.** Four paragraphs, including "There is no cmptne, so SETNE
  would otherwise reach instruction selection unlowered and kill the compiler
  with 'Cannot select'" and a paragraph about MPFR's `__gmpfr_ceil_log2`
  assembling a double in [1,2). The MPFR anecdote is then repeated verbatim in
  `fcmp.ll:57-60`. Keep the *why* (no `cmptne` instruction exists, so `SETNE`
  must be expanded), drop the narration and the duplication.
- `fcmp.ll` is inconsistent: `@oeq`/`@olt`/`@ogt` check the `srl $0, 62`; `@ole`,
  `@olt_f32`, `@ne_f32` omit it for no stated reason.
- Ten condition codes are set to Expand; the test covers oeq/olt/ogt/ole and
  une. `SETUO`, `SETO`, `SETONE`, `SETUEQ` are untested.

## `eabc77df741a` — Select floating-point select via fcmovne

- **`selcmp` test is misleading** (Summary item 2). At this commit the sequence
  is `cmptlt; stt; ldq; srl; stq; ldt; fcmovne` — two memory round trips — and
  the `[[C]]` binding matches only through incidental register reuse. Either
  land `45dfd5063fae`'s direct `select`-of-`fcmp` pattern here, or write the
  test so it shows the round trip honestly.
- `FCMOVNE` is `AlphaInst` with `Opcode = 0x17` and no function code (0x02b).
- The comment "(its bits are zero or non-zero)" glosses over the real subtlety:
  the condition value 1 is fed to `fcmovne` as the T_floating bit pattern
  0x0000000000000001, a denormal. It is safe only because FCMOVxx tests the
  raw value like FBNE and signals no FP exceptions. That is the WHY worth
  writing down; instead the file has WHAT comments elsewhere.
- `@seli` checks `fcmovne {{\$f[0-9]+}}, $f17, $f0` — the wildcard first operand
  makes the check nearly unfalsifiable.

## `9ace8008e4c4` — Enable unsigned integer/floating conversions

- The commit does two things: enable `UINT_TO_FP`/`FP_TO_UINT` expansion, and
  add five NaN-don't-care `FCmpPat`s. The message explains the dependency, but
  the `FCmpPat` block is a natural part of `dce08602d2ed`.
- `uint-fp.ll:5` — "Just check that they select and use the cvt instructions."
  The test then checks `subt`/`addt` for `@uitofp` (no `cvt` at all, despite the
  comment) and a bare `cvttq/c` for `@fptoui`. Both are close to vacuous: any
  plausible expansion contains those mnemonics, and neither verifies the
  correction step that makes the unsigned conversion correct for values with
  bit 63 set. A value-checking test (or at least CHECKs pinning the compare and
  select that implement the correction) would be worth much more.
