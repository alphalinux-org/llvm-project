# Alpha series — series-level review

Scope: 278 commits, `0bf3638..HEAD` (branch `alpha-triple-0bf3638`), 461 files,
+29755/-88. This file covers whole-series structure. Per-commit findings are in
`review-chunk-01.md` .. `review-chunk-10.md`.

## 0. BLOCKING: the series does not compile at `f69138566f06`

Verified directly. `f69138566f06` ("[lld] Implement multi-GOT for Alpha")
*uses* `GK_DtpOff`:

```
$ git grep -n GK_DtpOff f69138566f06 -- lld/ELF
f69138566f06:lld/ELF/Arch/Alpha.cpp:217:    kind = GK_DtpOff;
```

That is the only occurrence at that commit — the enumerator itself does not
exist. It is added by the *next* commit, `6164068a5336` ("[lld][Alpha] Support
R_ALPHA_GOTDTPREL"):

```
$ git grep -n GK_DtpOff 6164068a5336 -- lld/ELF
6164068a5336:lld/ELF/Arch/Alpha.cpp:56:  GK_DtpOff,   // the symbol's module-relative offset (R_ALPHA_GOTDTPREL)
6164068a5336:lld/ELF/Arch/Alpha.cpp:219:    kind = GK_DtpOff;
6164068a5336:lld/ELF/Arch/Alpha.cpp:330:  case GK_DtpOff:
6164068a5336:lld/ELF/Arch/Alpha.cpp:427:                    int64_t(getGotEntry(sym, 0, GK_DtpOff)), &sym});
```

lld does not build at `f69138566f06`. Fix by moving the enumerator (and its
`case GK_DtpOff:` arm) back into `f69138566f06`, or by reordering the two
commits.

**This is why the series needs intermediate build gates, not a tip-only check.**
A tip-only invariant is exactly what let this through, and it is the second time
that has happened on this branch. Nothing else in this review was found by
building — no commit-by-commit build was run — so there may be more. Gate the
whole range before submitting:

```sh
git rebase --exec '<configure && ninja lld llc clang>' 0bf3638
```

## Highest-priority structural issues

### 1. The ten tail cleanup commits must not ship as separate commits

Positions 269–278 are a de-LLM-ification and repair pass over the whole series:

| # | commit | subject |
|---|--------|---------|
| 269 | 99d967450c2a | Give four tests something to assert, and fix what one of them found |
| 270 | a0b52b0bc228 | Put the values back into six tests that only checked the shape |
| 271 | 8364a194250d | Check the ordering and the sign extension these two tests left open |
| 272 | c009f8859dab | Test the paths nothing reached |
| 273 | 7ebcdbf4b16c | Correct four rationales and delete code that cannot run |
| 274 | c9a6ae54bb9d | Format the series with clang-format |
| 275 | 8a36984bc6a4 | Move the insertions to their sorted positions |
| 276 | cbee60b78c50 | Keep each explanation in one place |
| 277 | aa91963c93a0 | Drop comments that restate the code below them |
| 278 | 4790ba4c5343 | Drop the tutorial preambles and name the test functions |

Every one of these is a retroactive fix to code introduced earlier in the same
series. Shipping them separately tells a reviewer, in the commit log, that the
first 268 commits were written unformatted, with unsorted table insertions,
with duplicated explanations, with restating comments, with tutorial preambles,
and with tests that asserted nothing. That is exactly the signal to remove.

`c9a6ae54bb9d` alone touches 63 files with 1167 insertions — it is a
whole-series reformat. `7ebcdbf4b16c` has a twelve-bullet message enumerating
twelve unrelated repairs across the series.

Action: squash each hunk into the commit that introduced the code it touches.
The one exception to check is any hunk in `99d967450c2a` / `7ebcdbf4b16c` that
is a genuine behavioural fix rather than a cleanup — see `review-chunk-10.md`.

Bisection consequence: at present, every commit from 12 to 273 fails
clang-format, and several carry code paths that are dead or tests that assert
nothing. Per-patch upstream review will hit each of those.

### 2. Commit-message subjects that count things

`Correct four rationales`, `Put the values back into six tests`,
`Check ... these two tests`, `Give four tests something to assert`. LLVM
subjects say what changed, not how many places it changed in. These counts only
exist because the commits are cleanup sweeps — they disappear once the hunks are
squashed back.

### 3. The clang ABI is landed wrong and corrected 130 commits later

The Alpha clang ABI is established at:

- 82 `191e891a25c2` [clang][Alpha] Add target support
- 84 `ef83df82488e` Give Clang the {base, offset} va_list
- 86 `55f8ff30af5a` Pass aggregates by value in the ABI

and then corrected at:

- 176 `ec861e1e8e2e` Pass long double by reference
- 181 `9ac03124f47e` Extend sub-64-bit arguments and returns
- 214 `96c198333e47` Fix the ABI for __int128 and complex types

Positions 82–213 therefore contain a knowingly-wrong ABI. `96c198333e47` is
titled "Fix the ABI", which in a fresh-target series should never appear: there
is no released compiler whose ABI is being fixed. Fold 176, 181 and 214 into
82/84/86 so the ABI is right the first time it appears.

### 4. Related flags scattered far apart

- 175 `0443b4690da9` `-mlong-double-128` vs 215 `8ef85f7d1945` `-mlong-double-64`
  — 40 commits apart, same flag family, same file. Make them adjacent.
- 9 `61e17ecc3c70` [clang] Diagnose the _BitInt suffix as an extension in GNU C
  modes vs 216 `2ecc1daa2618` [clang][Alpha] Enable _BitInt support up to 64
  bits. The generic change sits 200 commits before the Alpha work that
  motivates it. Either move it adjacent to 216, or make its message stand on
  its own without reference to Alpha (it is a generic C conformance fix and can
  be justified independently — but then it should go upstream as its own patch,
  not as part of this series).

### 5. Generic pre-requisites are mixed into the series

Positions 1–11 and the three `[MC]` st_other commits (chunk 8) and
`b12c5aca8d46` [SelectionDAG] Pass the value to ShouldShrinkFPConstant are
generic changes to shared code. Upstream will want these as separate,
independently-reviewable patches landed before the backend, not buried inside a
278-commit target series. Recommend splitting the series into:

1. Generic prerequisites (TailCallElim, TargetParser, BinaryFormat, MC,
   SelectionDAG, the three clang conformance fixes) — separate review.
2. The Alpha backend proper.
3. The out-of-tree consumers (lld, lldb, JITLink, compiler-rt, libunwind,
   OpenMP, sanitizers).

Note `313d580e0349` [TailCallElim] Make the frame-address test able to fail
comes *after* `6f4096c225c5` [TailCallElim] Do not mark a call tail when it is
handed the frame. A test-strengthening commit that demonstrates the bug must
come before the fix, or be squashed into it — otherwise the fix commit ships
with a test that passes either way. See `review-chunk-01.md`.

### 6. Docs commit placement

`440f811fc7d4` [docs] Announce the Alpha backend and list its references sits at
position 268, after the whole backend and all the consumers, but before the ten
cleanup commits. Once the cleanup commits are squashed away it becomes the tip,
which is the right place for it.

## Whole-series scans — clean results

These were checked across the full diff and found clean; recording so they are
not re-checked:

- No `TODO`, `FIXME`, `XXX`, or `HACK` introduced anywhere in the series.
- Added comments contain essentially no hedging or narration markers. A grep for
  `Note that|Importantly|This ensures|It is important|In other words|Essentially|
  Simply put|We need to|Here we|This is because|obviously|Basically` over every
  added line found two hits, both legitimate:
  - `AlphaSubtarget`-area comment referencing `gas/config/tc-alpha.c`
    ("Note that its CIX bit covers both of ours") — a factual statement about
    GNU as, correctly placed.
  - the `+cix`/`+fix` split comment in the same area.
  Commits 276–278 evidently did this sweep already; the point is that the sweep
  should be folded back, not that it is incomplete.
- 20 of 278 commit messages use bullet lists. Of the top offenders, six are the
  tail cleanup commits, which disappear on squash. The remainder
  (`bd89c4b75bce`, `af0eb12ffa9a`, `af082d9112d0`, `9ca05e38507d`,
  `fb38cbbc3e28`, `0a0690123ab2`) are enumerating genuinely list-shaped facts
  (which directives are accepted, which encodings are used) and are acceptable,
  though prose would read more like the rest of LLVM.

## Minor

- `llvm/lib/Target/Alpha/AlphaAsmPrinter.cpp:158` —
  `report_fatal_error("Alpha operand lowering is not yet implemented")` in the
  `default:` arm of `lowerOperand`. Other targets word this as "unknown operand
  type"; "not yet implemented" implies a planned gap that does not exist. From
  `5eb60e54dd1a`.

## Not verified

- **Bisectability.** No commit-by-commit build was run. Given commit 274
  reformats 63 files and commits 269–273 repair code across the series, and
  given prior experience on this branch where a tip-only invariant hid a build
  break across 40 commits, the series should be gated at intermediate points
  before submission — not only at the tip.

## 7. Private tracker IDs in 32 commit messages

32 of the 278 commit messages reference an internal issue tracker that no
upstream reader can resolve:

```
Fixes ALPHA-001. ... Fixes ALPHA-030.
Fixes ALPHA-T01. ... Fixes ALPHA-T07.
Addresses ALPHA-L03, ALPHA-L05 and ALPHA-L06.
Addresses ALPHA-024, which reported the two as contradictory.
Fixes ALPHA-010 and the feature-check half of ALPHA-011.
Fixes ALPHA-021, and the ext-free.ll half of ALPHA-T02.
```

List them with:

```sh
for h in $(git log --format=%h 0bf3638..HEAD); do
  git log -1 --format=%b $h | grep -qE 'ALPHA-[A-Z]?[0-9]' && git log -1 --oneline $h
done
```

Two problems.

First, LLVM's convention is a GitHub issue link (`Fixes
https://github.com/llvm/llvm-project/issues/NNNNN`) or nothing. A bare
`ALPHA-017` is dead weight to every reader.

Second and more telling: the numbering describes the *process that produced the
series*, not the code. `ALPHA-001` through `ALPHA-030` are a review-finding
queue, `ALPHA-T01`..`ALPHA-T07` a test-audit queue, `ALPHA-L03`..`ALPHA-L06` a
lint queue. Phrases like "the ext-free.ll half of ALPHA-T02", "the schedule.ll
half of ALPHA-T04" and "the feature-check half of ALPHA-011" show commits being
carved to close tracker items rather than to make a coherent change. "The rest
of ALPHA-011 does not reproduce" is a note to the tracker, not to a reader of
the log.

Strip all 32 references. Where a reference is doing real work — explaining *why*
a change is being made — replace it with the substance of the report, e.g.
`ALPHA-024, which reported the two as contradictory` becomes a sentence saying
what the contradiction was.

## 8. Verified: wrong CMP function codes carried for 51 commits, corrected under an "NFC" claim

Cross-chunk (chunks 2 and 3), verified directly against the tree.

`7ce4f183a39f` (#18, "Select integer comparisons (setcc)") defines:

```
def CMPEQ  : CMP_rr<0x10, "cmpeq">;
def CMPLT  : CMP_rr<0x10, "cmplt">;
def CMPLE  : CMP_rr<0x10, "cmple">;
def CMPULT : CMP_rr<0x10, "cmpult">;
def CMPULE : CMP_rr<0x10, "cmpule">;
```

`CMP_rr`'s first parameter is `bits<7> func` — a *function* code — in that
commit and at HEAD alike. All five therefore carry the same value, and it is
wrong for every one of them. The correct codes, as they read at
`AlphaInstrInfo.td:514-518` today, are `0x2d/0x4d/0x6d/0x1d/0x3d`. `0x10` is the
integer-arithmetic *opcode*, not a function code — the neighbouring `ALU_rr`
definitions in the very same commit get their `(opcode, func)` pairs right, so
this is a placeholder that was never filled in.

It is corrected 51 commits later in `5e5e91b7aafb` (#70, "Add instruction
encoding formats"), whose message ends:

> No functional change to assembly output; this only populates the Inst bits
> that the MC code emitter will consume.

That is literally true — there was no encoder before #70, so the wrong value was
inert and no test could have caught it — but the commit silently repairs five
wrong function codes while claiming NFC. Reviewed per-patch, #70 looks like a
mechanical format refactor; it is not.

Checked the rest of `5e5e91b7aafb`'s encoding changes: everything else it
touches is either `AlphaInst` (carrying *no* encoding) being given a real format
class, or pure whitespace (`FP_rr<0x080,"adds"` to `FP_rr<0x080, "adds"`). The
FP function codes were correct from the start. The CMP block is the sole case of
a knowingly-wrong value.

Action: put the correct function codes in `7ce4f183a39f` where the instructions
are first defined. If for some reason they must stay placeholder, say so in that
commit's message and drop the "no functional change" claim from `5e5e91b7aafb`.

## 9. `cmpxchg i32` is broken for ~75 commits mid-series (not at HEAD)

Cross-chunk (chunks 3 and 5), verified. `review-chunk-03.md` reports this as an
unqualified bug; the correction is that it is a *bisect window*, not a live
defect.

- `a3cd1ae11ccf` (#46, "Lower atomic compare-and-swap") gives
  `ATOMIC_CMPXCHG_I64` an unconstrained `atomic_cmp_swap` fragment with no
  `MemoryVT`, so an i8/i16/i32 CAS selects the 64-bit `ldq_l/stq_c` loop.
- `ebda925ff7b8` (#64, "Lower sub-word compare-and-swap") adds constrained
  I64/I16/I8 fragments — but no I32 — converting the silent miscompile into a
  "Cannot select" failure.
- `ee52db3535d2` (#121, "Support 4-byte atomics with ldl_l/stl_c") finally adds
  `ATOMIC_CMPXCHG_I32` and the test that covers it
  (`cmpxchg ptr %p, i32 %e, i32 %n seq_cst seq_cst`).

At HEAD all four fragments are present (`AlphaInstrInfo.td:1764-1767`) and
`cmpxchg` is tested at i8, i16, i32 and i64. The problem is confined to commits
#46–#120, where `cmpxchg i32` first miscompiles and then fails to select.

That window matters for two reasons: bisecting any unrelated Alpha atomics bug
through it will hit a spurious failure, and a per-patch upstream reviewer
reading #46 sees a fragment that is wrong on its face. Constrain the fragment to
i64 in `a3cd1ae11ccf` where it is introduced, and let each later commit add its
own width.

## 10. "Verified under qemu-alpha" appears in 55 commit messages

This is largely a *strength* — the series carries real execution evidence, and
several of the notes are specific and useful:

> the relocations and that the result links and runs under qemu-alpha for ...
> Verified under qemu-alpha: floor, sin and pow return correct ...
> under qemu-alpha with misaligned 2-, 4- and 8-byte accesses.

But the bare form — `Verified under qemu-alpha.` as a standalone closing
sentence — recurs often enough to read as a template rather than a claim about
that particular patch. Where the sentence does not say *what* was verified, it
tells the reader nothing and should be dropped; where it does, keep it.

## 11. Every one of the 278 commit messages ends in 20–23 blank lines

Verified across the whole series:

```sh
for h in $(git log --format=%h 0bf3638..HEAD); do
  git show -s --format=%B $h | tac | awk 'NF{exit} {c++} END{print c+0}'
done | sort -n | uniq -c
#  122 20
#   21 21
#  135 23
```

Not one message is free of it. Nothing a person types by hand produces exactly
20, 21 or 23 trailing blank lines on 278 consecutive commits — this is a
generation artifact, and it is the single most mechanical tell in the series. It
also survives `git log` display, so any reviewer scrolling the branch sees it
immediately.

Strip with a filter-branch / `git rebase --exec` pass over the whole range, or
during the squash work items 1–3 above already require.

## 12. Verified: residual `!Other` guard bug in the generic st_other work

`review-chunk-08.md` reports this; confirmed at HEAD in
`llvm/lib/MC/ELFObjectWriter.cpp`, at both sites:

```c
uint8_t Other = Symbol.getOther();
if (!Other)
  Other = resolveAliasedOther(
      Symbol, Asm.getContext().getAsmInfo().getInheritedSTOtherMask());
```

(`ELFObjectWriter.cpp:462` and `:1234`.)

`MCSymbolELF::getOther()` returns the full stored five-bit field shifted back up
(`MCSymbolELF.cpp:173-175`), so `!Other` asks "are *any* st_other bits set",
while the inheritance it guards is scoped by `getInheritedSTOtherMask()`. A
symbol that carries any st_other bit *outside* the mask therefore skips
inheritance of the masked bits entirely, even though those bits are unset. The
guard and the operation disagree about which bits they are talking about.

It should test the masked bits only:

```c
uint8_t Mask = ...getInheritedSTOtherMask();
if (!(Other & Mask))
  Other |= resolveAliasedOther(Symbol, Mask);
```

This is generic MC code affecting every ELF target, so it needs a test that
exercises a symbol with a non-mask st_other bit already set.

## 13. Minor series-wide items (verified)

**A `???` lab note ships in the scheduling model.**
`llvm/lib/Target/Alpha/AlphaSchedule.td:405`:

> `// ??? An itof result handed straight to an ftoi costs 8 cycles rather than`

An unresolved question mark left in committed code. Either establish the number
and state it, or drop the comment.

**One commit message argues with a hypothetical reviewer.**
`0e0d39c89651` ("Add scheduling models for the 21064, 21164 and 21264/21364"):

> Keeping the three in one commit is deliberate: they share the bypass and ...

Commit messages describe the change, not the packaging decision. The other five
uses of "deliberate/deliberately" in the series (`2e2cf5c2b94e`, `35ece8bbaefa`,
`58230c70ee79`, `9ca05e38507d`, `60f44ff8bf5e`) are all technical statements
about the code and should stay.

## Reading order for the fix pass

1. `review-series-level.md` items 1–3 (squash the tail, fix the ABI ordering)
   and item 11 (trailing blank lines) — these reshape the series and should be
   done before any per-commit edits, because they move hunks between commits.
2. Item 7 (strip 32 tracker IDs) and item 11 — mechanical, do in the same
   rebase.
3. The per-chunk files, oldest chunk first, applying correctness fixes into the
   commits that introduce the code.
4. Re-check items 8, 9 and 12 last: each is a cross-commit defect whose fix
   lands in an early commit but is only observable later.
