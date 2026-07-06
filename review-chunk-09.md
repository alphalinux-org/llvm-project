# Review: commits 221–250 (`0bf3638..HEAD`), Alpha backend series

## Summary

**Blocking**

1. **`f69138566f06` ("[lld] Implement multi-GOT for Alpha") does not compile.**
   `lld/ELF/Arch/Alpha.cpp:217` uses `GK_DtpOff`, which is not declared until the
   *next* commit, `6164068a5336`. This is a hard bisection break in the middle of
   the highest-risk area of the series. The `GK_DtpOff` enumerator and the
   `case R_ALPHA_GOTDTPREL:` arm of `gotSlotsFor` must move back into
   `f69138566f06`.

2. **`0c435020bd35` sets deliberately wrong `Size` values on `JSR`/`JSRd`/`OTS_CALL`
   and its commit message asserts they are right** ("Also give the call instructions
   the sizes they have always had"). `802ced3d6201`, the very next commit, corrects
   all three and explains that the wrong values make branch relaxation decline to
   relax an unreachable branch. Squash `802ced3d6201` into `0c435020bd35` and drop
   the false sentence.

3. **`04ed9c907c5c` lands an ASan shadow offset (`0x70000000000`) that cannot be
   mapped on a real Alpha kernel**, with a test asserting that value; `fd2873f333a4`
   replaces it two commits later. Squash.

**Squash-back candidates** (fixups to earlier commits in this same chunk):
`78e6313e075f` → `a2cab01755e0`; `35ece8bbaefa` → `6ec35f580082`;
`7505274f0eb6` → `c1d2ccf2ef73`; `362243ec3752` → `f69138566f06`/`6164068a5336`/`c1d2ccf2ef73`;
`802ced3d6201` → `0c435020bd35`; `fd2873f333a4` → `04ed9c907c5c`;
`a48782af2efc` → `6ec35f580082` (arguable).

**Correctness findings worth acting on** (detail below): unsound `+8` gp-load skip
when a literal carries a non-zero addend (`6ec35f580082`); silent `R_NONE` for GOT
relocations in unscanned sections (`f69138566f06`); an implicit "relocations are in
offset order" invariant that `relaxTlsCall` can violate and `startsWithGpLoad`
depends on (`6b83cd87ba43`); an unchecked deref of the faulting PC
(`338fd6144cad`); `value.SetUInt` on IEEE754-encoded FP registers (`04f0bbab0240`).

**Pervasive style problems**: seven commit messages carry internal tracker IDs
(`Fixes ALPHA-022.`, `ALPHA-016`, `ALPHA-T04`, `ALPHA-014`, `ALPHA-023`, `ALPHA-024`,
`ALPHA-008`, `ALPHA-007`) and one an internal patch number (`#118`). None of these
mean anything upstream and all must go. Several comments explain test coverage or
narrate the author's discovery process rather than the code.

---

## `b8f7b798eb79` — [Alpha] Do not widen a float with cvtst

Correct. `getFPTrapFuncBits`/`getFPTrapFuncBitsForSpelling` both already map `"s"`
to `0x400`, and `CVTST` (`TrapClass = 5`, func `0x2ac`) + `0x400` = `0x6ac`, which
matches the separately-defined `CVTST_S`. The `NotIEEE`/`HasIEEE` pattern split is
complete: `AlphaInstrInfo.td:688` (`FPEXTST`, NotIEEE) and `:774`
(`Pat<(f64 (fpextend f32:$Rb)), (CVTST $Rb)>`, HasIEEE).

Nits:

- The `FMov` class is moved from above `FAbs` to below `FNeg` with no functional
  reason. Pure churn in a diff that is otherwise about semantics — drop the move.
- A stray blank line is inserted after `def FPEXTST` (`AlphaInstrInfo.td:689`).
- `llvm/test/CodeGen/Alpha/fp-constant.ll:15` replaces a positive `CHECK: cvtst`
  with `CHECK-NOT: cvtst` between `CHECK: lds` and `CHECK: addt`. That is a real
  check, but the test now has no `-mieee` RUN line, so the `HasIEEE` half of every
  pattern this commit adds is untested. Add a second RUN with `-mattr=+ieee`
  checking `cvtst/s`.

## `a2cab01755e0` — [lld] Add Alpha ELF support for static linking

Relocation arithmetic checks out: `checkInt(disp, 23)` for the 21-bit longword
BRADDR/BRSGP field, `relocateGpDisp`'s `(disp >> 16) + ((disp >> 15) & 1)` borrow
and its `[-0x80000000, 0x7fff8000)` range, the `STO_ALPHA_STD_GPLOAD == 0x88` /
`STO_ALPHA_NOPV == 0x80` mask test (matches bfd), and `R_ADDEND` for the
symbol+addend literal (`val - gpBias` == `gotOff - 0x8000`, correct).

- **`InputSection.cpp` `getTlsTpOffset`, EM_ALPHA arm**: `s.getVA(ctx, 0) +
  alignTo(16, std::max<uint64_t>(tls->p_align, 1))` is exactly what the generic
  variant-1 default computes from `ctx.target->tlsHeaderSize`. Set
  `tlsHeaderSize = 16` in the `Alpha` constructor and delete the `case EM_ALPHA`
  instead of adding a target to a switch that already has a correct default.
- **`lld/test/ELF/alpha-gp-overflow.s:2-3`**: the header claims the test covers
  "a GOT that grows past the 64KB a single gp can address", but the only `CHECK`
  is for `R_ALPHA_GPREL16 out of range`. The `"GOT displacement out of range;
  multi-GOT is not implemented"` diagnostic in `relocate()` is never exercised.
  Either add the case or trim the comment.
- `finalizeRelocScan`'s `Err(ctx) << "Alpha does not support dynamic linking yet"`
  and the multi-GOT diagnostic are removed one and two commits later. That is
  legitimate incrementalism, but note that both are *user-visible strings* churned
  within a single series.
- `Relocations.h`: `RE_ALPHA_GPDISP` is inserted between `RE_AARCH64_GOT_PAGE_PC`
  and `RE_AARCH64_GOT_PAGE`, splitting the AArch64 block. Move the ALPHA entries
  after it (this compounds in `f691`/`6ec3`/`c1d2`, which add four more).

## `949b9148e118` — [lld] Support dynamic linking for Alpha

- **`Driver.cpp` `setConfigs`**: `ctx.arg.writeAddends = ... || ctx.arg.emachine ==
  EM_ALPHA` makes `--no-apply-dynamic-relocs` a silent no-op on Alpha. If that is
  intended, say so; if not, gate the in-place write on `R_ALPHA_RELATIVE` only.
- **`getLiteralGotOffset`**: `Err(ctx) << "R_ALPHA_LITERAL against preemptible
  symbol '" << &sym << "' with a non-zero addend"` is the only diagnostic in the
  file with no `getErrorLoc(ctx, ...)` prefix, so it is unlocatable. It also has no
  test.
- **`R_ALPHA_BRADDR`/`R_ALPHA_BRSGP` change from `R_PLT_PC` to `R_PC`** is the
  headline behavioural change ("a branch cannot reach a preemptible symbol, and
  that is diagnosed"), and nothing in `alpha-shared.s` or `alpha-tls-shared.s`
  exercises the resulting diagnostic. Add it.
- **`getImplicitAddend` is missing.** This commit turns on RELA *and*
  `writeAddends` for Alpha; an assertions build validates dynamic addends by
  reading them back, which needs `getImplicitAddend`. That override only arrives in
  `adbd0c2797f1`, five commits later, whose own message says exactly why it is
  needed. Move it here.
- **Triplicated prose.** The same three sentences ("a call loads the callee's
  address from the ordinary GOT with R_ALPHA_LITERAL and jumps to it, so the PLT
  exists only as a lazy-binding trampoline: R_ALPHA_JMP_SLOT relocates the .got
  slot, not a .got.plt slot") appear in the file header comment, in the constructor
  comment above `gotPltHeaderEntriesNum = 0`, and again in the commit message. Keep
  one — the file header — and make the constructor comment a one-liner.

## `f69138566f06` — [lld] Implement multi-GOT for Alpha

**Does not build.** `lld/ELF/Arch/Alpha.cpp:216-218`:

```
  case R_ALPHA_GOTDTPREL:
    kind = GK_DtpOff;
    return 1;
```

`GK_DtpOff` is not a member of the `GotKind` enum added at lines 53-59 of this
commit; it is added by `6164068a5336`. Move it (and the `gotSlotsFor` arm) back.

Other findings:

- **`getRelExpr` silently returns `R_NONE`** for `R_ALPHA_LITERAL`,
  `R_ALPHA_TLSGD`, `R_ALPHA_TLSLDM`, `R_ALPHA_GOTTPREL`, with the comment "if one
  turns up in a section that is not scanned (.eh_frame or a non-alloc section)
  there is nothing sensible to compute." Silently writing nothing is worse than
  erroring: the field keeps whatever the assembler left there. Emit an
  `Err(ctx) << getErrorLoc(...)` instead.
- **`scanSectionImpl`'s `if (sec.file != curFile)`** is a change-detector, not a
  membership test. It is correct only while all of a file's scanned sections are
  contiguous. If that ever stops holding (special-section scanning, a future
  reordering), the file is re-measured and `partOfFile[curFile]` is *overwritten*,
  silently relocating already-scanned sections against a different gp. Either
  assert the invariant (`assert(!partOfFile.count(sec.file))`) or key off
  `partOfFile` directly.
- `allocGot`'s undercount check reports via `InternalErr` and lets the link
  continue; the `checkInt(val, 16)` in `relocate()` will catch the fallout, so this
  is fine, but it means the internal error is advisory. Worth a one-line note.
- Partition capacity: `maxEntriesPerPart = 0x10000/8 = 8192` gives a last entry at
  partition offset 65528, displacement +32760 — fits. Correct.
- `RE_ALPHA_GOT` is not in the `isStaticLinkTimeConstant` `oneof<>` list, which is
  fine only because Alpha bypasses `rs.process()` for these. Worth a comment so a
  future refactor doesn't reintroduce the hole.

## `6164068a5336` — [lld][Alpha] Support R_ALPHA_GOTDTPREL

Correct in itself. The `GK_DtpOff` enum addition belongs in `f69138566f06` (above).

- `AlphaFixupKinds.h:35-38`: the comment block now describes both `gottprel` and
  `gotdtprel` but sits above `fixup_alpha_gottprel` only, so the new enumerator
  reads as undocumented. Split into two comments.
- `llvm/test/MC/Alpha/tls-reloc.s` checks relocation types and symbol names but no
  offsets or addends. Adequate for a specifier-parsing test.

## `adbd0c2797f1` — [lld][Alpha] Support ifuncs with R_ALPHA_IRELATIVE

The design (IRELATIVE straight onto the GOT slot, no PLT) is sound and both failure
modes are diagnosed and tested.

- `getImplicitAddend` should be in `949b9148e118` (see above); this commit's own
  message argues the case.
- Only `R_ALPHA_REFQUAD` gets the read-only-section ifunc check. `R_ALPHA_REFLONG`
  and the `R_ALPHA_SREL*` family against an ifunc fall through to the generic path
  with no diagnostic. Either extend the check or say why they cannot occur.
- `alpha-ifunc.s` covers only the executable case. An ifunc in a shared object is
  untested.

## `78e6313e075f` — [lld][Alpha] Diagnose !samegp against a symbol with no prologue marking

Correct, matches bfd's three-way switch, and the wording matches GNU ld.

- **Squash into `a2cab01755e0`.** That commit introduced the two-way test and the
  `alpha-branch.s` test that covers the two valid markings; this is its missing
  third arm. Nothing between the two depends on the buggy behaviour.
- **`Fixes ALPHA-022.`** — remove.
- The commit message's "alpha-branch.s already covers both valid markings; the new
  test covers the third case, which nothing did" is coverage bookkeeping; drop it
  once squashed.

## `63073b6938b2` — [Alpha] Tag a direct tail call for relaxation

Encoding and fixup ordering are right (`std::rotate` puts LITUSE before HINT,
matching GNU as and what bfd inspects).

- **`llvm/test/CodeGen/Alpha/tailcall.ll`**: the comment claims "a hint only when
  the callee is not dso-local. An indirect tail call has neither", but the RELOC
  prefix has three `CHECK`/`CHECK-NEXT` lines and no terminating `CHECK-NOT`, so
  neither the dso-local case (LITUSE, no HINT) nor the indirect case (no
  relocations) is actually asserted. Add `RELOC-NOT` coverage or trim the comment.
- `AlphaISelLowering.h:44-48`: one comment now covers `TC_RETURN_DIRECT`,
  `TC_RETURN_DIRECT_LOCAL` and `TC_RETURN_BR`, and `TC_RETURN_BR` loses the
  one-line description it had. Give each its own line.
- The `std::rotate` in `encodeInstruction` runs unconditionally for `TCRETURNdl`,
  where there is no hint fixup to reorder. Harmless, but a `if (Op == TCRETURNd)`
  guard would say what is going on.

## `4fa520dcb620` — [Alpha] Complete the scheduling models

Setting `CompleteModel = 1` on all three models is exactly the right lasting change,
and the `MULL`/`MULLi` gap it exposes is a real bug with a real test.

- **`Fixes ALPHA-016, and the schedule.ll half of ALPHA-T04.`** and **"#118 then
  turned on the post-RA scheduler"** — internal identifiers, meaningless upstream.
  Describe the change instead ("a later commit turns on the post-RA scheduler for
  EV4 and EV5").
- **`AlphaSchedule.td`: `def : InstRW<[Wr_IALU], (instrs COPY)>;` with the comment
  "is gone before any scheduler sees it" is factually wrong.** `COPY` survives
  coalescing and is very much visible to the pre-RA machine scheduler; it is
  `ExpandPostRAPseudos` that removes it, and that runs before the post-RA scheduler,
  not before the pre-RA one. The mapping is a reasonable choice; fix the
  justification.
- `schedule.ll`: `imul32_hide` and `fdiv_hide` have `INORDER-NEXT` lines but no
  `EV6` counterparts, so for the ev6 RUN both reduce to `CHECK: %bb.0:` … `CHECK:
  ret` and assert nothing about scheduling. Either add `EV6-NEXT` expectations or
  restrict those two functions to the in-order RUN lines.

## `0c435020bd35` — [Alpha] Load a call's procedure value at the call

The core change is right and important, the instruction constants are all correct
(`0xa77d0000` = `ldq $27,0($29)`, `0x6b5b4000` = `jsr $26,($27)`, `0x6bfb0000` =
`jmp $31,($27),0`), and `call-literal-per-call.ll` is a good test.

- **The `Size` values introduced here are wrong**: `JSR` is `Size = 8` but emits
  jsr + the two-word ldgp expansion (12); `JSRd` is `Size = 12` but emits ldq + jsr
  + ldgp (16); `OTS_CALL` is `Size = 8` but emits 12. The commit message calls them
  "the sizes they have always had", which is false — they are new and wrong.
  `802ced3d6201` fixes all three. **Squash the two and drop the sentence.**
- No test covers the dso-local (`JSRdl`) relocation shape — LITUSE with no HINT.
  `call-literal-per-call.ll` only checks the external callee.

## `802ced3d6201` — [Alpha] Declare the real size of the multi-instruction call pseudos

Good change; the `scope_exit` guard in `encodeInstruction` is the right mechanism.

- **Squash backwards into `0c435020bd35`** (above).
- The assert is `Declared == 0 || CB.size() - StartSize == Declared`. Since
  `MCInstrDesc::getSize()` is 0 for anything that doesn't set `Size` explicitly, a
  future multi-word pseudo that forgets `Size` is *not* caught, contrary to the
  message's "a future pseudo that grows an instruction fails on the first test that
  encodes it". Either say "a pseudo that declares a size" or make the guard also
  assert `Declared != 0` for the multi-word opcodes it knows about.
- **`Fixes ALPHA-014.`** — remove.

## `6ec35f580082` — [lld] Implement --relax for Alpha

The relaxation itself is sound in the common case. Verified: `INSN_UNOP =
0x2ffe0000` is `ldq_u $31,0($30)`; the `isInt<23>(disp)` gate matches the 21-bit
longword BRADDR field; `disp` is measured from the instruction after the branch in
both the gate and `relocate()`; the `pv` register match `((insn >> 16) & 31) != pv`
correctly rejects a call through some other register; `getGp(c.sec->file) ==
getGp(dsec->file)` is the right multi-GOT guard and is tested by
`alpha-relax-multi-got.s`.

Findings:

- **Unsound `+8` with a non-zero literal addend.**
  `int64_t addend = c.addend + (skipGpLoad ? 8 : 0);` uses `d.stOther`, which
  describes the function at `d.value`, not the entry at `d.value + c.addend`. For a
  call literal against `sym+N` with `N != 0` and `sym` marked `STO_ALPHA_STD_GPLOAD`,
  the branch lands at `sym+N+8`, eight bytes past an arbitrary instruction. In
  practice gas emits `section_symbol + offset` (st_other 0) so the case does not
  arise today, and `6b83cd87ba43` handles the GPDISP-detected path correctly, but
  the `st_other` path should require `c.addend == 0`.
- **`c.sec->relocations[relocIndex.find(c.litOffset)->second]`** dereferences an
  unchecked `DenseMap::find`. It cannot fail today (the literal always adds a
  relocation at that offset), but a bare `->second` on a `find` result is the kind
  of thing an upstream reviewer will flag. Use `at()`/`lookup` with an assert.
- **`finalizeRelax(int passes)` ignores `passes`.** Drop the parameter name or use
  it. (`6e3a2641c60c` replaces this with `relaxOnce(int pass)` and does use it.)
- `relocate()`'s `RE_ALPHA_RELAX_JSR` arm has no `checkInt` — safe only because
  `finalizeRelax` pre-checked and Alpha relaxation does not resize sections. That
  stops being true in `6e3a2641c60c`, which adds the check. Consider adding it here.
- **No test of the HINT-carrying layout.** `35ece8bbaefa` adds it and says so.
  Squash that test into this commit.
- `alpha-relax.s` and `alpha-relax-nonlocal.s` are good tests: `--no-relax`
  counterparts, an out-of-range case, a `lituse_base` case that pins the load, and
  preemptible/ifunc negatives. The displacement arithmetic in every `##` comment
  checks out.

## `a48782af2efc` — [lld] Relax Alpha tail calls

Correct: `func != FUNC_JSR && func != FUNC_JMP` correctly excludes `ret` (2) and
`jsr_coroutine` (3), and `relocate()` picks `OP_BR` vs `OP_BSR` from the original
function field while preserving Ra, giving `br $31, disp` for a jmp.

- Consider squashing into `6ec35f580082`: this rewrites the comment and condition
  that commit just added, and edits the same test's expectations. As a standalone
  commit it is defensible; as a series being prepared for upstream it is noise.

## `35ece8bbaefa` — [lld][Alpha] Correct the HINT ordering comment and link a real hinted call

**Squash into `6ec35f580082`.** This corrects a comment that commit wrote and adds
the test it should have had.

- **`Addresses ALPHA-023.`** — remove.
- The replacement comment in `Alpha.cpp` spends five lines explaining that the arm
  it guards is untestable ("which is why no test can reach this arm … It is here so
  that the order stays a detail of the producer rather than something correctness
  rests on"). Production code should not narrate its own test coverage. Two lines:
  "A HINT shares the jsr's offset, so it does not end the group. Both GNU as and our
  emitter write the LITUSE first; do not depend on that."
- `alpha-relax-hint.s` repeats the same three paragraphs in its header. Trim to the
  first two sentences.
- `# CHECK: bsr $26, 2` uses `CHECK:` not `CHECK-NEXT:`, so the test never asserts
  what happened to the `ldq` preceding it. That is deliberate here (`callee` is
  unmarked so the load stays), but a `CHECK-NEXT` on the retained `ldq` would make
  the test say so.

## `c1d2ccf2ef73` — [lld] Relax Alpha dynamic TLS sequences

The most intricate commit in the chunk. Encodings verified: `INSN_RDUNIQ =
0x0000009e` is `call_pal rduniq`; `INSN_ADDQ_TP = 0x42000400` is `addq $16,$0,$0`
(opcode 0x10, Ra 16, func 0x20 in bits 11-5); `memInsn` produces zero-displacement
ldah/lda/ldq correctly. The five-instruction LE and IE rewrites both land the result
in `$0`, matching `__tls_get_addr`'s return register. The `ctx.arg.isPic` gate and
the `p0 + 4 == p1` adjacency requirement for splitting the offset across ldah/lda
are both correct, and the "something in between" fallback is tested.

Findings:

- **No opcode validation on `p0`.** `unsigned arg = (read32le(sec.content().data()
  + p0) >> 21) & 31;` reads bits 25-21 of whatever is at `p0` without checking it is
  an `lda`, and the IE rewrite then hardcodes `Rb = 29` in `memInsn(OP_LDQ, arg,
  29)`. If the TLSGD relocation is not on `lda $arg, x($29)` the rewrite is silently
  wrong. Add `if ((read32le(...) >> 26) != OP_LDA || ((read32le(...) >> 16) & 31)
  != 29) return false;`.
- **The `!sym.isPreemptible` test in the LD path is meaningless.** For
  `R_ALPHA_TLSLDM` the symbol is ignored — the result is always this module — and
  the whole rewrite is already confined to a non-PIC executable, where nothing is
  preemptible. Either drop the condition for `!isGd` or comment why it is kept.
- **The "RELAX_INSN before the immediate relocation at the same offset" ordering is
  an implicit invariant.** It holds because `sec.addReloc` appends and `flush()`
  uses `stable_sort`, but nothing enforces or asserts it, and the `Relocations.h`
  comment states it as a fact rather than a requirement. An `assert` in `relocate()`
  or a comment at the `addReloc` calls would be cheap insurance.
- **`relaxTlsCall` is a ~90-line lambda inside `scanSectionImpl`.** Make it a member
  function; the lambda captures nothing that a member could not reach except `rs`
  and `canRelax`.
- The `gpdisp` search `while (++gpdisp != rels.end() && gpdisp->r_offset <=
  gpdispOff)` followed by a redundant post-loop re-test is correct but convoluted.
  A straight `if (++gpdisp == rels.end() || gpdisp->r_offset != gpdispOff ||
  gpdisp->getType(false) != R_ALPHA_GPDISP) return false;` says the same thing, given
  that offsets are ascending.
- The `canRelax` prescan duplicates the `curLit` grouping logic in a second loop
  over `rels`. Factor or comment the duplication.
- `alpha-relax-tls.s` and `alpha-relax-shared-literal.s` are both good, targeted
  tests; the `NORELAX` prefix in the former checks only three lines, which is thin
  but sufficient given the positive checks.

## `362243ec3752` — [lld][Alpha] Keep the addend on the GOT-based TLS relocations

A real bug fix — `ldq $1, x+8($29) !gottprel` resolved to `x` — with a test that
covers three distinct addends plus reuse of a repeated one.

- **Squash backwards.** The `GK_TpOff`/`GK_DynTls` arms and the `R_ALPHA_GOTTPREL`/
  `R_ALPHA_TLSGD` call sites belong in `f69138566f06`; the `GK_DtpOff` arm and the
  `R_ALPHA_GOTDTPREL` call site in `6164068a5336`; the `relaxTlsCall` IE arm in
  `c1d2ccf2ef73`. Nothing in this chunk should ship with the wrong behaviour.
- **`Fixes ALPHA-008.`** — remove.
- **Verify the `addAddendOnlyRelocIfNonPreemptible` replacement.** The new
  `ctx.in.relaDyn->addReloc(/*isAgainstSymbol=*/false, tlsGotRel, got, off, sym,
  addend, R_ABS, symbolicRel)` computes the written addend from `R_ABS` against
  `sym`, whereas the helper it replaces computes it via `addendRelType`. For a
  non-preemptible TLS symbol in a shared object the value written must be the
  offset within the module's TLS block. Confirm the `R_ABS` path produces that and
  not the symbol's link-time VA; there is no test for the shared-object,
  non-preemptible `gottprel+N` case.
- The message narrates the investigation ("the inconsistency was internal", "No
  existing test used a non-zero addend on any of these"). Once squashed this all
  disappears; if kept standalone, compress to two sentences.

## `7505274f0eb6` — [lld][Alpha] Explain why the relaxed TLS sequence adds $16

Pure comment. **Squash into `c1d2ccf2ef73`.**

- **`Addresses ALPHA-024, which reported the two as contradictory.`** — remove; it
  is a pointer into a tracker no upstream reader can follow.
- **"No functional change; this is the reasoning, written down where it was
  missing."** — self-narration; delete.
- The reasoning itself is correct and worth having: the compiler's `mov $arg, $16`
  sits between `p1` and `p2` and is not part of the rewritten window, so `$16` holds
  the offset either way. Note the residual hazard the comment does not mention: if
  the compiler hoisted `p0`/`p1` far from `p2`, the ordering check `p0 < p1 < p2 <
  p3` still passes while intervening code could clobber `$arg`. bfd has the same
  hole, but say so.

## `6e3a2641c60c` — [lld] Drop the GOT load for an Alpha callee that never reads it

The highest-risk commit in the chunk. The reclamation algorithm is, as far as I can
follow it, correct:

- `remap[i] = kept * 8` is recorded before slot `i`'s fate is decided, so `move()`
  returns a kept slot's new offset and, for a dropped slot, the next kept slot's —
  which is exactly what the `erase_if` predicate `remap[off/8] == remap[off/8 + 1]`
  needs. The `slots + 1` sizing makes the last-slot lookup safe.
- No live `RE_ALPHA_GOT` relocation can be remapped onto the wrong entry, because an
  entry is dropped only when every literal load reading it had its `expr` changed to
  `RE_ALPHA_RELAX_INSN` and therefore no longer matches the remap filter.
- TLS pairs occupy two consecutive slots and are never in `dropped`, so stepping one
  slot at a time never lands inside a live entry. Stated in the comment; correct.
- `litDynamic` is populated for exactly the `GK_Addr` cases that get a dynamic
  relocation (ifunc, preemptible, PIC), so only pure constants are reclaimed.
- The separation of `skipGpLoad` (needs matching gp) from `dropLoad` (needs
  `onlyJsrUses` *and* NOPV-or-skipGpLoad) is right, and the `.Lbase` test case
  correctly now skips the callee's gp load while keeping the caller's GOT load.
- `alpha-relax-nonlocal.s`'s EXE case exercises `moveDyn` for real (an IRELATIVE at
  `.got+0` after two constant entries were reclaimed).

Findings:

- **"Nothing here changes a section's size, so one pass settles it." is wrong.**
  `reclaimGot` shrinks `.got` — that is the whole point, and the commit message says
  so ("That moves .got, and everything laid out after it"). Rewrite the comment to
  say what is actually true: the *relaxation decisions* are made in one pass, and
  `.got` shrinking only moves things earlier, so no already-relaxed branch can go out
  of range. The `checkInt` added to `relocate()` covers the residual case.
- **`relaxOnce` is `const` and mutates state.** `gotSize` and `partOfFile` were made
  `mutable` specifically to allow this, and the method also mutates
  `ctx.in.got->relocations`, `sec->relocations` across `ctx.inputSections`, and the
  dynamic relocation vectors. `mutable` to defeat a `const` interface is a smell
  upstream reviewers will push back on; either make `relaxOnce` non-const in
  `TargetInfo` or move the mutable state out of the target.
- **`moveDyn` covers `relaDyn->relocs`, `relaDyn->relativeRelocs` and
  `relaPlt->relocs`, but not `relaPlt->relativeRelocs`.** Confirm nothing can land
  there for Alpha (the ifunc path uses `getIRelativeSection` with
  `isAgainstSymbol=false`, which goes to `relocs`), and add a comment or an assert
  either way — an offset left un-remapped here corrupts the GOT silently.
- **`GotSection::dropEntriesAfter(size_t entries) { numEntries = entries; }`** is a
  raw setter on shared synthetic-section state with a name that reads as "drop
  everything after index N" while the argument is a count. Rename
  (`setNumEntries`/`truncateTo`) and document that the caller is responsible for
  having remapped every reference.
- The comment `// bfd additionally recognizes an unmarked callee whose first two
  words carry a GPDISP; we take the marking at face value.` is removed by the very
  next commit. Fine, but it is a "TODO in prose" that lives for exactly one commit.

## `6b83cd87ba43` — [lld] Recognize an unmarked Alpha callee that loads gp

Materially valuable (20556 → 6714 GOT entries on the kernel) and correctly uses
`d.value + c.addend` rather than `d.value`, which also incidentally closes the
addend hole flagged under `6ec35f580082` for this path.

- **`startsWithGpLoad` assumes `sec.relocations` is sorted by offset** ("Relocations
  are kept in offset order, so the first one at or past the entry point is the only
  one that can be it") and uses `llvm::lower_bound` on that basis. That invariant is
  not enforced anywhere, and `relaxTlsCall` (`c1d2ccf2ef73`) can break it: it appends
  relocations for `p0…p4` while processing the relocation at `p0`, so any relocation
  in `(p2, p4)` that is not in `tlsRelaxed` is appended *after* offset `p4`. The
  `flush()` in `relaxOnce` only sorts sections that appear in `relaxCalls`, and
  `startsWithGpLoad` is called on the *callee's* section, which need not be one of
  them. Either sort the section's relocations unconditionally before the relax pass,
  or replace `lower_bound` with a linear scan.
- `alpha-relax-gpload.s` is a good test: it specifically covers the local-function
  case where the GPDISP to look for is at `d.value + addend` and not at offset 0 of
  the section.

## `b2d1f72554d3` — [compiler-rt] Build the builtins for Alpha

- **`__asm__ volatile("imb")` needs a memory clobber**: `__asm__ volatile("imb" :::
  "memory")`. Without it nothing stops the compiler from sinking the stores of the
  just-written instructions past the barrier. (Several neighbouring arches in
  `clear_cache.c` have the same omission; that is a reason to note it, not to repeat
  it.)
- The commit message's second sentence runs three clauses through a nested
  em-dash aside and then tacks on "— and implement `__clear_cache` with the imb PAL
  call". Split it.

## `87cbdaa9036b` — [libunwind] Add Alpha support

Sizing checks out: `_LIBUNWIND_CONTEXT_SIZE 65` = 33 GPR slots + 32 FP slots, and
the `static_assert(sizeof(_registers) == 0x108)` pins the FP base at 264. The
`jumpto` restore order is correct — `$28` is loaded before the two `.irp` loops,
neither of which includes 28, 31 or 16, and `$16` is restored last.

- **`mov $28, $26` clobbers `$26` after restoring it** is a deliberate ABI
  deviation, justified at length in both the code and the message (gcc landing pads
  rebuild `$gp` from `$26`). The justification is sound but this is the kind of
  thing an upstream libunwind reviewer will want called out in the *summary line*,
  not buried in paragraph four. Consider a shorter, sharper message with the
  deviation stated up front.
- The `jumpto` comment reproduces the commit message's fourth paragraph nearly
  verbatim. One of the two should be a pointer to the other.
- `getRegisterName` returns `"$pc"`/`"$sp"` while every other libunwind port returns
  bare names (`"pc"`, `"sp"`). Cosmetic but visible in `unw_regname` output.
- No test. libunwind's suite has no arch-specific register tests, so this is
  consistent with the tree, but the `_LIBUNWIND_CURSOR_SIZE 77` figure is unverified
  by anything.

## `481f0d2c90ff` — [sanitizer_common] Build and test the Alpha runtimes with -mieee

Correct and minimal. No findings. (Note the author date is identical to
`b8f7b798eb79`'s, which is harmless but suggests the two were split after the fact.)

## `754769a43ddf` — [sanitizer_common] Walk an Alpha stack with unwind tables

- **The `message(FATAL_ERROR)` will be contentious upstream.** Hard-failing a
  configure for a whole architecture because a library choice was not made is
  heavier than anything else in `compiler-rt/CMakeLists.txt`. The escape hatch
  (`COMPILER_RT_UNWINDER_LINK_LIBS`) is thoughtfully provided, but a
  `message(WARNING)` plus disabling the sanitizers would be the more conventional
  shape. At minimum expect to defend this.
- `sanitizer_stacktrace.h`: the new `#elif SANITIZER_ALPHA` block uses `#  define`
  (two spaces) while the adjacent mips and Windows arms use `# define` (one). Match
  the neighbours.

## `04ed9c907c5c` — [asan] Add the Alpha shadow mapping

- **The offset added here, `0x70000000000`, is unusable.** With scale 3 the shadow
  spans `[0x70000000000, 0x78000000000)`, entirely above Alpha's `TASK_SIZE` of
  `0x40000000000`; `fd2873f333a4` says so and replaces it. `alpha-shadow.ll` asserts
  `add {{.*}} 7696581394432`, i.e. the test encodes the wrong value.
  **Squash `fd2873f333a4` into this commit.**
- The comment "the existing OrShadowOffset test already selects an add rather than
  an or" is correct for both values (7<<40 and 3<<40 are not powers of two).
- `CHECK-NOT: or i64` is bounded between the `add` and `ret`, so it is a real check,
  not a vacuous one.

## `fd2873f333a4` — [asan] Put the Alpha shadow inside the address space

The new layout is arithmetically correct: with offset `0x30000000000` and scale 3,
LowMem `[0, 0x2ffffffffff]`, LowShadow `[0x30000000000, 0x35fffffffff]`, gap
`[0x36000000000, 0x36fffffffff]`, HighShadow `[0x37000000000, 0x37fffffffff]`,
HighMem `[0x38000000000, 0x3ffffffffff]` — exactly the table added to
`asan_mapping.h`, and `TASK_SIZE/2 = 0x20000000000` does fall in LowMem.

- **Squash into `04ed9c907c5c`** (and into whichever earlier commit introduced
  `ASAN_SHADOW_OFFSET_CONST` for `SANITIZER_ALPHA` in `asan_mapping.h` — that is
  outside this chunk).
- **`Fixes ALPHA-007.`** — remove.
- The paragraph "This is verifiable only against the kernel constant: qemu-alpha in
  user mode does not enforce the guest TASK_SIZE … alpha-shadow.ll only checks that
  the compiler agrees with the runtime constant, so it cannot catch this either" is
  useful information but belongs in the source comment (where a shortened form
  already is), not in a commit message that will be squashed away.

## `338fd6144cad` — [sanitizer_common] Say whether an Alpha fault was a read or a write

Every opcode is correct: 0x0a ldbu, 0x0b ldq_u, 0x0c ldwu, 0x0d stw, 0x0e stb,
0x0f stq_u, 0x20-0x23 ldf/ldg/lds/ldt, 0x24-0x27 stf/stg/sts/stt, 0x28-0x29 ldl/ldq,
0x2a-0x2b ldl_l/ldq_l, 0x2c-0x2d stl/stq, 0x2e-0x2f stl_c/stq_c. Excluding lda/ldah
is right and is explained.

- **`u32 instruction = *(u32 *)ucontext->uc_mcontext.sc_pc;` dereferences the
  faulting PC unguarded.** On a SIGSEGV caused by an instruction-fetch fault — a
  call through a null or wild function pointer, which is a case ASan exists to
  report — this faults a second time inside the signal handler. The mips arm does
  the same thing, which is precedent but not an argument; note the hazard at least,
  and consider guarding with `IsAccessibleMemoryRange`.
- The switch body is indented one level from `switch`; LLVM style puts `case` at the
  same indentation as `switch`.
- No new test; the message cites the existing `segv_read_write` test, which is
  reasonable.

## `e3bd0643c34f` — [clang][Alpha] Do not advertise MemorySanitizer

Correct, minimal, tested. The only nit is that the `CHECK-ASAN-ALPHA` prefix is
reused for the `-fsanitize=undefined` RUN line, so the name misdescribes half its
uses — `CHECK-SUPPORTED-ALPHA` would be clearer.

## `ebccac577e35` — [OpenMP] Add support for Alpha

The `__kmp_invoke_microtask` assembly is correct. Traced: the 32-byte frame with
`$26` at 0, `$15` at 8, gtid at 16, tid at 20; CFI via `$15` so the later dynamic
`subq $30, $1, $30` does not invalidate the CFA; `max(0, argc-4)` slots rounded to
16; `argv[0..3]` into `$18..$21` with the correct early exits at each step; the
spill loop starting at `argv[4]` and `0($sp)`; `$27` set to the callee for gp
derivation. `argc = 5` traces through correctly. The syscall numbers 395/396 are
Alpha's `sched_setaffinity`/`sched_getaffinity`.

- **Add `.usepv __kmp_invoke_microtask, no`** (or `.ent`/`.prologue 0`). The
  function does not establish `$29` and does not declare that fact, so `st_other`
  stays 0 — and `78e6313e075f`, earlier in this same chunk, makes a `!samegp`
  branch to exactly such a symbol a hard link error. The series should not ship
  hand-written assembly that its own linker change diagnoses.
- The `KMP_MB()` paragraph is the most valuable part of the message and is well
  argued. The evidence sentence — "The built library has 2161 `mb` instructions, 148
  of them in kmp_barrier" — is a measurement, not a rationale; drop it or move it
  below a `---`.
- The commit message is eight paragraphs for a change that is mostly arch-list
  plumbing. The two "worth calling out" items justify their length; the rest could
  be one paragraph.

## `f2d67b1544e8` — [lldb] Add the Alpha register contexts and ArchSpec entry

Layout and numbering are right: GPR note is 33 quadwords ($0-$29, usp, pc, unique),
`fpcr` occupies $f31's slot, DWARF 0-30 / 32-62 / 63 (fpcr) / 64 (pc) / 66 (unique)
matches GCC's `DEBUGGER_REGNO`, and the generic-register assignments ($15 FP, $26
RA, $30 SP, $16-$21 ARG1-6) are correct. The three unit tests are specifically named
(`CoreNoteOffsets`, `DwarfNumbering`, `GenericRegisters`) and assert real values.

- **`DEFINE_GPR_NODWARF` is defined at `RegisterInfos_alpha.h:72` and `#undef`'d at
  :177 but never used.** Dead code; delete it.
- `$31` has no `RegisterInfo` at all, so DWARF register 31 is unmappable. Correct
  (it reads as zero and is not in the note), but worth one line in the header
  comment so a future reader does not think it was forgotten.
- `CMakeLists.txt`: `RegisterContextPOSIX_alpha.cpp` is inserted after
  `RegisterContextPOSIX_riscv32.cpp`, breaking the alphabetical order of the
  surrounding block. `RegisterContextLinux_alpha.cpp` is placed correctly.

## `04f0bbab0240` — [lldb] Read an Alpha ELF core file

- **`value.SetUInt(v, reg_info->byte_size)` on floating-point registers is wrong.**
  The FP `RegisterInfo`s declare `eEncodingIEEE754`/`eFormatFloat`, but `SetUInt`
  makes the `RegisterValue` a `eTypeUInt64` holding the bit pattern. `register read`
  happens to print correctly because it formats via `reg_info`, which is why the
  shell test passes, but `RegisterValue::GetAsDouble()` will numerically convert
  `0x3FE0000000000000` rather than bit-cast it. Use
  `value.SetFromMemoryData(*reg_info, ptr, size, byte_order, error)` as
  `RegisterContextPOSIXCore_arm64` and friends do.
- **The test header overclaims**: "check that lldb reads the register notes and
  unwinds", but there is no backtrace or frame check — only `register read --all`
  and `thread list`. Either add a `thread backtrace` check (the commit adds the pc
  and RA numbering precisely so unwinding works) or drop "and unwinds".
- `ThreadElfCore.cpp`: both new `#include`s are inserted out of alphabetical order
  (`RegisterContextLinux_alpha.h` after `_i386.h`, `RegisterContextPOSIXCore_alpha.h`
  after `_riscv64.h`). clang-format will move them.
- The `if (offset == reg_info->byte_offset + reg_info->byte_size)` success test is
  an unusual way to detect a short read; a direct bounds check against
  `m_gpr.GetByteSize()` would read better.
- `ReadAllRegisterValues` returning `false` unconditionally matches the other
  elf-core contexts.
