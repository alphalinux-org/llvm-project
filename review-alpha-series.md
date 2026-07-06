# DEC Alpha backend series — consolidated review

Reviewed range: `0bf3638..HEAD` on branch `alpha-triple-0bf3638` — 278 commits,
461 files, +29755/-88.

Produced by eleven passes: ten reviewers over contiguous commit ranges, plus one
whole-series structural pass. Findings marked **verified** were re-checked
directly against the tree; the rest are a reviewer's report and should be
confirmed before acting on them.

Map a commit position to a hash with:

```sh
git log --reverse --format='%h %s' 0bf3638..HEAD | nl -ba | sed -n '<N>p'
```

## Contents

1. [Blockers](#1-blockers)
2. [Series-level findings](#2-series-level-findings) — structure, ordering,
   squash/split, series-wide tells
3. [Priority findings by area](#3-priority-findings-by-area)
4. [Per-commit findings](#4-per-commit-findings) — in series order

---

## 1. Blockers

Fix these before anything else. Neither was found by building — no
commit-by-commit build was run as part of this review — so there may be more.

**The series does not compile at `f69138566f06`** (verified). It uses
`GK_DtpOff`, which the next commit defines. See
[series-level item 0](#0-blocking-the-series-does-not-compile-at-f69138566f06).

**A second bisect break at `beb2e35cdf43`** — it adds a `-mlong-double-128`
"no error" check one commit before `0443b4690da9` makes it stop erroring. See
the per-commit entry for `beb2e35cdf43`.

Gate the whole range before submitting:

```sh
git rebase --exec '<configure && ninja lld llc clang>' 0bf3638
```

### Structural work, before any per-commit edits

These move hunks between commits, so they come first:

- **Squash the ten tail cleanup commits** (positions 269-278) into the commits
  whose code they repair — series-level item 1, and the per-commit entries for
  the two hunks that must survive separately.
- **Fix the clang ABI ordering** — landed wrong at 82-86, corrected at
  176/181/214 — series-level item 3.
- **Strip 32 private tracker IDs and the 20-23 trailing blank lines on all 278
  messages** — series-level items 7 and 11. Mechanical; same rebase.
- **Consider splitting the series** into generic prerequisites / backend /
  consumers for upstream review — series-level item 5.

---

## 2. Series-level findings

Scope: 278 commits, `0bf3638..HEAD` (branch `alpha-triple-0bf3638`), 461 files,
+29755/-88. This file covers whole-series structure. Per-commit findings are in
`review-chunk-01.md` .. `review-chunk-10.md`.

### 0. BLOCKING: the series does not compile at `f69138566f06`

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

### Highest-priority structural issues

#### 1. The ten tail cleanup commits must not ship as separate commits

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

#### 2. Commit-message subjects that count things

`Correct four rationales`, `Put the values back into six tests`,
`Check ... these two tests`, `Give four tests something to assert`. LLVM
subjects say what changed, not how many places it changed in. These counts only
exist because the commits are cleanup sweeps — they disappear once the hunks are
squashed back.

#### 3. The clang ABI is landed wrong and corrected 130 commits later

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

#### 4. Related flags scattered far apart

- 175 `0443b4690da9` `-mlong-double-128` vs 215 `8ef85f7d1945` `-mlong-double-64`
  — 40 commits apart, same flag family, same file. Make them adjacent.
- 9 `61e17ecc3c70` [clang] Diagnose the _BitInt suffix as an extension in GNU C
  modes vs 216 `2ecc1daa2618` [clang][Alpha] Enable _BitInt support up to 64
  bits. The generic change sits 200 commits before the Alpha work that
  motivates it. Either move it adjacent to 216, or make its message stand on
  its own without reference to Alpha (it is a generic C conformance fix and can
  be justified independently — but then it should go upstream as its own patch,
  not as part of this series).

#### 5. Generic pre-requisites are mixed into the series

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

#### 6. Docs commit placement

`440f811fc7d4` [docs] Announce the Alpha backend and list its references sits at
position 268, after the whole backend and all the consumers, but before the ten
cleanup commits. Once the cleanup commits are squashed away it becomes the tip,
which is the right place for it.

### 7. Private tracker IDs in 32 commit messages

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

### 8. Verified: wrong CMP function codes carried for 51 commits, corrected under an "NFC" claim

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

### 9. `cmpxchg i32` is broken for ~75 commits mid-series (not at HEAD)

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

### 10. "Verified under qemu-alpha" appears in 55 commit messages

This is largely a *strength* — the series carries real execution evidence, and
several of the notes are specific and useful:

> the relocations and that the result links and runs under qemu-alpha for ...
> Verified under qemu-alpha: floor, sin and pow return correct ...
> under qemu-alpha with misaligned 2-, 4- and 8-byte accesses.

But the bare form — `Verified under qemu-alpha.` as a standalone closing
sentence — recurs often enough to read as a template rather than a claim about
that particular patch. Where the sentence does not say *what* was verified, it
tells the reader nothing and should be dropped; where it does, keep it.

### 11. Every one of the 278 commit messages ends in 20–23 blank lines

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

### 12. Verified: residual `!Other` guard bug in the generic st_other work

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

### 13. Minor series-wide items (verified)

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

### Whole-series scans — clean results

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

### Minor — assembler-printer wording

- `llvm/lib/Target/Alpha/AlphaAsmPrinter.cpp:158` —
  `report_fatal_error("Alpha operand lowering is not yet implemented")` in the
  `default:` arm of `lowerOperand`. Other targets word this as "unknown operand
  type"; "not yet implemented" implies a planned gap that does not exist. From
  `5eb60e54dd1a`.

### Not verified

- **Bisectability.** No commit-by-commit build was run. Given commit 274
  reformats 63 files and commits 269–273 repair code across the series, and
  given prior experience on this branch where a tip-only invariant hid a build
  break across 40 commits, the series should be gated at intermediate points
  before submission — not only at the tip.

### Reading order for the fix pass

1. `review-series-level.md` items 1–3 (squash the tail, fix the ABI ordering)
   and item 11 (trailing blank lines) — these reshape the series and should be
   done before any per-commit edits, because they move hunks between commits.
2. Item 7 (strip 32 tracker IDs) and item 11 — mechanical, do in the same
   rebase.
3. The per-chunk files, oldest chunk first, applying correctness fixes into the
   commits that introduce the code.
4. Re-check items 8, 9 and 12 last: each is a cross-commit defect whose fix
   lands in an early commit but is only observable later.

---

## 3. Priority findings by area

Each reviewer's own prioritized list for the range it covered. Detail for every
item is in [section 4](#4-per-commit-findings).

### Generic prerequisites (commits 1–11)

1. **`313d580e0349` must be squashed into `6f4096c225c5`.** It exists only to fix a
   vacuous test that `6f4096c225c5` introduced. Shipping "here is a broken test" followed by
   "here is the fix" is not an upstreamable split. The message also contains
   `Fixes ALPHA-T01.`, a reference to a private tracker that has no meaning upstream.
2. **`05090f24c5b7` silently changes the meaning of a public constant.**
   `ELF::EM_ALPHA` goes from `41` to `0x9026`. Any out-of-tree code using `ELF::EM_ALPHA`
   keeps compiling and starts meaning something else. This is the single most contentious
   change in the chunk and needs to be called out explicitly in the message.
3. **`05090f24c5b7` picks readobj names where one is a prefix of the other**
   ("Digital Alpha" / "Digital Alpha (standard)") while the test's check line is
   `# CHECK: Machine: [[MACHINE]]` — unanchored. The `EM_ALPHA` RUN line therefore
   cannot distinguish the two values.
4. **`3caa5a0a3714` has no test at all**, and its `static_assert(ELF_IsMemoryTagged_Shift == 15)`
   asserts a literal against itself — it cannot catch the overflow the comment describes.
   The comment admits this ("which this cannot check for you").
5. **`dad7b844f932`'s test is `expected-no-diagnostics` end to end**, so it also passes if
   `-Wuninitialized` stops working entirely. Its commit message also understates the
   change's scope (it affects 7 builtins, not 4).

---

### Backend skeleton and first-wave instruction selection (commits 12–40)

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

### Stack arguments, atomics, varargs, division, jump tables (commits 41–69)

Highest-severity findings first.

1. **`a3cd1ae` / `ebda925`: `cmpxchg i32` is broken for ~20 commits.** From `a3cd1ae`
   until `ebda925`, `ATOMIC_CMPXCHG_I64`'s pattern uses the *unconstrained*
   `atomic_cmp_swap` fragment (no `MemoryVT`), so an i8/i16/i32 `cmpxchg` selects the
   64-bit `ldq_l/stq_c` loop — a silent miscompile (wrong access width, wrong
   compare). `ebda925` then constrains the fragments but only defines I64/I16/I8, so
   from `ebda925` until `ee52db35` (outside this chunk) `cmpxchg i32` fails isel with
   "Cannot select". Every point in the range is broken for `std::atomic<int>::CAS`,
   in two different ways. Also no test in this chunk covers `cmpxchg i32` at all.
2. **`ebda925`: sub-word cmpxchg reports the wrong success flag.** `$dst` is
   `extbl`/`extwl` output (zero-extended) while the outer `SETCC(dst, cmp)` that
   LegalizeDAG builds for `ATOMIC_CMP_SWAP_WITH_SUCCESS` compares against the
   *un-normalized* promoted `cmp` operand. `getExtendForAtomicCmpSwapArg()` is not
   overridden (default `ANY_EXTEND`), and i8/i16 arguments arrive sign-extended, so
   `cmpxchg ptr %p, i8 -1, ...` stores correctly but returns `success = false`. The
   test only checks instruction shape, so it cannot fail.
3. **`408a37e`: the va_list offset field is 64 bits wide; gcc's is 32.** gcc's
   `alpha_build_builtin_va_list` builds `{ char *__base; int __offset; }` with
   `integer_type_node`. `LowerVASTART`/`LowerVAARG`/`LowerVACOPY` store and load 8
   bytes at `va_list+8`, so a va_list produced by gcc code (padding undefined) is read
   with garbage in the high 32 bits. This breaks the "interoperating with gcc-compiled
   callers" claim in the commit message in exactly the direction not tested.
4. **`4ea758c` + `6328b37` must be squashed.** `4ea758c` lands a pattern-based pre-BWX
   byte/word store that `6328b37` documents as losing data ("lost most of ASan's
   default flag values"). Landing a known miscompile and repairing it two commits
   later is not bisectable and reviewers will ask for the squash.
5. **`b9255a7` must be squashed into `a5407c4`** for the same reason (seq_cst loads
   are unfenced in between), and the "Fixes ALPHA-006" trailer must go — LLVM uses
   GitHub issue numbers, not a private tracker.
6. **Instruction encodings are systematically deferred.** `MB`, `LDQ_L`, `STQ_C`,
   `LDL_L`, `STL_C`, `LDQ_U`, `STQ_U`, `EXTBL`, `EXTWL`, `INSBL`, `INSWL`, `MSKBL`,
   `MSKWL`, `BICi`, `LDLg`/`LDQg`/`LDTg`/`LDSg`/`ST*g`, `JMP`, `SQRTS`, `SQRTT`, and
   `CMOV_cc` are all introduced in this chunk as bare `AlphaInst` (only `Opcode` set)
   or as `OForm` with an unbound `Ra` — so sibling instructions encode *identically*
   (`EXTBL` == `EXTWL`, `SQRTS` == `SQRTT`, all three `CMOV_cc`) and `MB` encodes as
   `trapb`. All are repaired in later commits. Each instruction should carry its
   correct encoding in the commit that introduces it.
7. **Formatting**: several commits in this chunk are visibly not clang-formatted
   (multi-statement aligned `case` lines in `emitAtomicRMW`, `LowerDivRem`, and
   `EmitInstrWithCustomInserter`; >80-column lines in `LowerFormalArguments` and the
   `LD*g` defs), despite commit `c9a6ae54` ("Format the series with clang-format")
   earlier in the series.
8. **Commit-message style**: nearly every commit ends with "Verified end to end under
   qemu-alpha …". That is not an LLVM convention and reads as generated boilerplate;
   drop it or state it once in the series cover letter. Several commit messages also
   narrate the test diff ("atomic-load-store.ll checked the missing-barrier
   sequence…") — LLVM messages say *why*, not what the diff contains.

---

### MC layer, disassembler, assembly parser, clang target and ABI, TLS (commits 70–100)

Encodings were checked field-by-field against the Alpha Architecture Handbook
(opcode/function codes for operate, FP-operate, memory, branch, jsr and PALcode
formats) and the relocation numbers against binutils `include/elf/alpha.h`.
**All instruction encodings and all relocation numbers in this chunk are
correct**, and every hand-written byte string in the tests
(`elf-obj.ll`, `MC/Alpha/basic.s`, `MC/Disassembler/Alpha/alpha.txt`,
`pseudo-mov-clr.s`) decodes to what the CHECK line claims.

The problems are elsewhere:

1. **Two self-inflicted regressions that are only fixed 30-160 commits later.**
   - `68b1b88ea461` silently deletes `Size = 12` from `JSR`, which
     `d00ec074d435` had added one commit earlier. It is not restored until
     commit #231 (`802ced3d6201`). For 157 commits `JSR` claims to be 4 bytes
     while the emitter writes 12.
   - `44625d2bc56a` changes the constant-pool fallback condition from
     `!isInt<16>(Hi)` to `!isInt<32>(V)`, which drops 32-bit constants whose
     high half is `0x8000` (e.g. `INT32_MAX`) on the floor. They fall through
     to `SelectCode` and hit "Cannot select". Fixed at commit #129
     (`9b9e5711781b`), whose own message confirms the failure mode.
2. **A real object-correctness bug**: `88ea133580bc` emits the `lituse_jsr`
   relocation against a symbol literally named `.text`
   (`Ctx.getOrCreateSymbol(".text")`), not against the section the jsr is
   actually in.
3. **Undefined behaviour** in `buildConstantInline` (`V - Lo32` overflows
   `int64_t` for `V` near `INT64_MAX`).
4. **Parser crashes on malformed input**: `986dd46fcfe5`'s `ldgp`/`jsr`
   special cases call `getReg()`/`getMemBase()` on operands without checking
   the kind.
5. **Structure**: three later commits are pure fixups of earlier ones in the
   same chunk and should be squashed backwards
   (`149ee63f25c7` -> `ef83df82488e`; `a715d62c5180` -> `7f87b1815dfb`;
   the `Size = 12` restore -> `68b1b88ea461`). Two commit messages carry
   internal tracker IDs (`Fixes ALPHA-004.`, `Fixes ALPHA-013.`) that are
   meaningless upstream. One commit message (`5e5e91b7aafb`) describes files it
   does not touch.
6. Assorted dead code introduced and removed later (`def symbol`,
   `AlphaISD::THREAD_POINTER`), a generic MC change bundled into a target
   commit with no user in that commit, and several comments that contradict
   the code they sit on.

---

### Call-frame information, intrinsics and builtins, misaligned access, scheduling (commits 101–130)

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

### Cost model, tail calls, outliner, branch relaxation, assembler directives (commits 131–160)

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

### GP addressing, trap modes, f128 through OTS, long double (commits 161–190)

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

### Exception handling, fixups, assembler directives, st_other, i128 (commits 191–220)

Highest-severity findings, in order:

1. **`960dff3ca980` proves `e8d8565673d5` and `527b9b1ab286` were wrong as landed.**
   The first two propagate `st_other` through alias chains for *every* ELF target;
   the third documents that this silently gave RISC-V aliases `STO_RISCV_VARIANT_CC`.
   Bisecting to any commit in that 4-commit window miscompiles riscv64/aarch64/mips
   ABI metadata. These three must be **squashed into one commit** that introduces the
   per-target mask from the start. Upstream will not accept a knowingly-broken
   intermediate state in generic MC.
2. **`9ca05e38507d` does not deliver its headline claim.** `cvttq/svc` — the exact
   instruction the Alpha compiler emits for every float→int conversion under `-mieee`
   — still cannot be disassembled (`AlphaDisassembler.cpp` rejects it), and cannot be
   *assembled* correctly either: it silently encodes as `cvttq/sv`. Details below.
3. **`dd2e8a4e012e` documents a NaN semantics for `_OtsEqlX` that contradicts the
   adjacent pre-existing comment and, if the new comment is right, makes all six
   `Negate`-based unordered condition codes miscompile.** Only one of the two can be
   true and neither is tested with a NaN.
4. **`acf0a0870278` introduces an Alpha-only driver regression**: `Value.consume_front("-m")`
   mutates the loop variable before the `break` that falls through to generic
   `-Wa,` handling, so `-Wa,-mcpu=...`, `-Wa,-march=...`, `-Wa,-msoft-float` etc. are
   no longer recognised on Alpha and the diagnostic prints a mangled string.
5. **`3647f8a10e60`'s five `CHECK-NOT` lines are vacuous** — they sit before the first
   positive `CHECK`, so they only scan the region above the first function label.
6. Three later commits are pure fixes for bugs introduced 1–2 commits earlier and
   should be squashed backwards: `c7fc0b19fe2d` → `c63d11231dfb`,
   `58230c70ee79` → `09c0d33e6eac`, and the `getCallTargetEncoding` hunk of
   `af082d9112d0` → `9f2b6d35d9e6`.
7. Several commits are kitchen sinks and need splitting: `af082d9112d0` (5 unrelated
   bug fixes), `bd5c29e1b543` (FP qualifiers + 10 unrelated instruction defs),
   `75e8a679a184` (three directives + an undocumented fourth change),
   `5bb3fc961dbd` (new fixups + two `needsRelocateWithSymbol` policy changes, one
   undocumented), `0a630862b917` (f32↔f128 support + an unrelated MVT/EVT crash fix).

Series-wide, mechanical:

* **Every commit in this chunk has 20–23 trailing blank lines** in its message
  (`git log -1 --format=%B | cat -A`). Strip them before sending anything upstream.
* Six commits carry `Fixes ALPHA-0NN.` referring to a private tracker
  (`acf0a0870278`, `9ca05e38507d`, `c7fc0b19fe2d`, `960dff3ca980`, `58230c70ee79`,
  `af082d9112d0`). Upstream LLVM uses `Fixes #NNNNN` for github issues; a private ID
  is noise. `af082d9112d0` goes further and includes a paragraph of tracker triage
  ("The rest of ALPHA-011 does not reproduce…") that belongs in the tracker.
* MC/asm tests are inconsistent about comment syntax: `.s` files under
  `test/MC/Alpha/` mix `#`/`##` (`fp-qualifiers.s`, `usepv.s`) with `//`/`///`
  (`label-difference.s`, `jsr-target.s`, `reloc-directive.s`). Upstream `.s` tests
  use `#` for RUN/CHECK and `##` for prose.

---

### lld, compiler-rt, sanitizers, libunwind, OpenMP (commits 221–250)

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

### lldb, JITLink, GlobalISel, docs, and the tail cleanup commits (commits 251–278)

**Real correctness bugs found (must fix before submission):**

1. `ebc803f1d692` — `ABISysV_alpha.cpp:104-106`: sign extension is applied to the
   `Scalar` *before* the value is assigned to it, so it is unconditionally
   discarded. Every other lldb ABI plugin does it the other way round. Dead code
   that silently loses signedness for every negative integer argument.
2. `4026eec0de3f` — `G_FCMP` is made `legalFor` all predicates, but `selectFCmp`
   returns `false` for `FCMP_ORD`/`FCMP_UNO`. Those are producible from C
   (`isnan`, `x == x` under `-ffast-math`-free builds). With
   `-global-isel-abort=1` that is a hard "cannot select", not a fallback. No test
   covers either predicate.
3. `4026eec0de3f` — the code and both comments assert "a compare against a NaN is
   false"; `CMPTEQ`/`CMPTLT`/`CMPTLE` without the `/SU` qualifier *trap* on NaN
   on Alpha. `6ab5e7a4907f`'s own commit message confirms this target traps
   ("comparing it then took a floating-point trap"). Either the `/SU` forms are
   needed or the rationale is wrong.
4. `096a635cab37` — `getEntryForTargetAndAddend` keys on `Target.getName()`.
   Anonymous symbols have no name, so two distinct anonymous targets with the
   same addend collide onto one GOT entry. lld keys on the symbol, not its name.
5. `7ebcdbf4b16c` — deleting `AlphaISD::THREAD_POINTER` left its doc comment
   behind as an orphan attached to the *next* enumerator
   (`AlphaISelLowering.h:92-95`).
6. `b291259df038` is entirely superseded by `6a428ab4339d`, which rewrites the
   very comment `b291` adds. They cannot both ship.

**Series-level blockers:**

- **Internal tracker IDs in commit messages.** 34 commits across the whole
  series (8 in this chunk) end in `Fixes ALPHA-017` / `Fixes ALPHA-T03` /
  `Addresses ALPHA-L03, ALPHA-L05 and ALPHA-L06`. These reference a private
  review tracker and are meaningless upstream. All must go.
- **The ten tail commits should not ship as commits.** See the dedicated section.
- `99d967450c2a` mixes a real backend behaviour change (`definedByFP` phi walk)
  into a commit whose subject says it is a test commit.
- Commit messages throughout this chunk are 2–8 paragraphs of design prose with
  bulleted lists. LLVM commit messages are usually 1–3 short paragraphs.
  `05601bb7c1e3` is six paragraphs for a 1651-line commit.

---

---

## 4. Per-commit findings

In series order, oldest first. Headings are `<hash> — <subject>`.

### Generic prerequisites (commits 1–11)

#### `6f4096c225c5` — [TailCallElim] Do not mark a call tail when it is handed the frame

##### Correctness

The core change (feeding `llvm.frameaddress` / `addressofreturnaddress` / `sponentry` /
`stacksave` results into `AllocaDerivedValueTracker`) is right, and re-using the existing
tracker gets the transitive gep/phi/select/store-escape handling for free.

- `llvm/lib/Transforms/Scalar/TailRecursionElimination.cpp:239-241` — the comment on the
  loop now reads:

  > `// The local stack holds all alloca instructions, all byval arguments, and`
  > `// anything that names the frame itself: llvm.frameaddress and friends hand`
  > `// out a pointer into the frame a tail call tears down before the callee runs.`

  "llvm.frameaddress and friends" is imprecise for a comment that is the only in-tree
  documentation of *which* intrinsics are in the list. Either name them or drop the
  clause; the list is three lines below.

- No test covers the case that motivated the whole change reaching the readnone path
  (`TailRecursionElimination.cpp:329-340`). A `frameaddress` result passed to a
  `memory(none)` callee takes the `SafeToTail` loop, not `AllocaUsers`. It happens to be
  handled (the arg is neither a `Constant` nor an `Argument`), but nothing pins it.

##### Commit message

Good — it explains why, cites the reproducer, and does not narrate the debugging. Two nits:

- "gcc.c-torture/execute/frame-address.c aborts on this on every target" — "on this on"
  is a typo/awkwardness.
- The em-dash-delimited aside "`-- it passes __builtin_frame_address(0) to a call whose
  comment says it exists to prevent exactly the tail call that was happening --`" is one of
  three `--` pairs in an 11-line message. Trim to one.

---

#### `313d580e0349` — [TailCallElim] Make the frame-address test able to fail

##### Structure — **squash into `6f4096c225c5`**

This is a fix to the immediately-preceding commit's test. Upstream this is not two commits;
it is one commit with a working test. The commit message here is a narration of the author
discovering their own mistake ("which is to say the only test for this change could not
detect the change being reverted"), which has no place in the permanent history once
squashed.

##### Commit message

- `Fixes ALPHA-T01.` — a private tracker ID. Must be removed.
- "Rewriting the pass's output to mark all four calls tail and running the test over it
  passes" — this describes a manual verification the reader cannot reproduce.

##### Test content

- `llvm/test/Transforms/TailCallElim/frame-address.ll:5-9` — a five-line paragraph
  explaining how FileCheck's unanchored `CHECK:` interacts with `CHECK-NOT:`. This is
  general FileCheck lore, not information about this test. It belongs in the commit
  message (where it already is), not duplicated in the test file.
- `frame-address.ll:44-45`:

  > `; The pointer reaching the callee through a bitcast or a gep is still this`
  > `; frame's, so the call is still not a tail call.`

  There is no bitcast in the test, and with opaque pointers there cannot be one. The
  comment names something the test does not exercise.
- `frame-address.ll:39` — `; llvm.stacksave is in the same list in the pass and had no test
  at all.` This is a changelog entry about the state of a prior commit, sitting in a test
  file. After squashing it is meaningless.
- `CHECK: {{^}}  call void @callee` hard-codes the two-space instruction indent. The
  conventional form here is `CHECK-NOT: tail call` scoped by the surrounding
  `CHECK-LABEL`s, or regenerating with `utils/update_test_checks.py`. The `{{^}}` form
  works but every future reader has to re-derive why.

---

#### `fae618baf9c5` — [TargetParser] Add DEC Alpha triple

##### Correctness

All the exhaustive switches are covered: `getArchTypeName`, `getArchTypePrefix`,
`getArchTypeForLLVMName`, `parseArch`, `getDefaultFormat`, `getArchPointerBitWidth`,
`get32BitArchVariant`, `get64BitArchVariant`, `getBigEndianArchVariant`, `isLittleEndian`,
`getDefaultExceptionHandling`, `computeDataLayout`. `getLittleEndianArchVariant` correctly
needs no entry (it early-returns on `isLittleEndian()`).

- `llvm/lib/TargetParser/Triple.cpp:661` — only `alpha` parses. Real-world Alpha triples are
  overwhelmingly `alphaev56-*`, `alphaev67-*`, `alphaev6-*` (that is what GNU config and
  Debian emit). None of them parse. If that is deliberate, say so in the message; if not,
  it wants a `startswith("alpha")` arm like the arm/mips/ppc handling above it.

- `llvm/lib/TargetParser/TargetDataLayout.cpp:579` —
  `"e-m:e-p:64:64-i64:64-i128:128-n64-S128"`. `i64:64` and `i128:128` are the LLVM defaults
  and are redundant; several nearby targets spell them anyway, so this is a nit, but the
  layout has *no test*. `computeDataLayout` is not exercised for `alpha` anywhere in this
  commit — a datalayout typo would be caught only when the backend lands, ~250 commits later.

- `llvm/include/llvm/TargetParser/Triple.h:1195` — `isAlpha()` has no in-tree caller in this
  commit or (from a grep of the tree) anywhere. Adding unused public API to `Triple.h` will
  draw a request to drop it or to land it with its first user.

##### Tests

`llvm/unittests/TargetParser/TripleTest.cpp:89-97` adds one parse case. Missing: the
`get64BitArchVariant`/`get32BitArchVariant`/`getBigEndianArchVariant` round-trips (there are
dedicated `TEST`s for those in the same file that were not touched), and the data layout.

##### Commit message

The body is a bulleted list flattened into prose — "the ArchType enum, name/prefix
accessors, both LLVM-name and user-triple parse tables, pointer width (64), endianness
(little), 32/64-bit and big/little-endian variant maps, default object format (ELF),
default exception handling (DWARF CFI), and an isAlpha() predicate" — which restates the
diff rather than explaining anything. The second and third paragraphs (why the data layout
is what it is, and that this is groundwork) are the useful part. Cut the first paragraph
to a sentence.

---

#### `05090f24c5b7` — [BinaryFormat] Correct EM_ALPHA to the value real objects use

##### Correctness — the headline concern

Renaming `EM_ALPHA` (41) to `EM_ALPHA_STD` and redefining `EM_ALPHA` as `0x9026` is a
**silent semantic change to a public header**. Downstream code that does
`if (Header.e_machine == ELF::EM_ALPHA)` continues to compile and now tests a different
number. binutils does spell it this way, which is the strongest argument for the patch, but
the message needs to state the compatibility hazard outright rather than only justifying the
new value. Expect a reviewer to ask for `EM_ALPHA_UNOFFICIAL`/`EM_ALPHA_LINUX` instead, and
for `EM_ALPHA` to keep meaning 41.

- `llvm/include/llvm/BinaryFormat/ELF.h:162` —
  `EM_ALPHA_STD = 41,     // DEC Alpha (standard, unused in practice)`.
  "unused in practice" is an assertion the header cannot back up; Compaq/Tru64 tools did emit
  41 in some configurations. Prefer stating what it is (the gABI-assigned value) and letting
  the 0x9026 definition carry the "this is what you'll actually see" note, which it already
  does at `ELF.h:328-331`.

- `llvm/lib/BinaryFormat/ELF.cpp:44` — `.Case("alpha_std", EM_ALPHA_STD)`. This invents a
  user-visible architecture string. `convertArchNameToEMachine` feeds `llvm-ifs --arch` and
  `IFSHandler` (`llvm/lib/InterfaceStub/IFSHandler.cpp:197`), so `alpha_std` becomes a legal
  value in `.ifs` text stub files, forever. It is untested and no producer will ever emit it.
  Drop it unless there is a concrete need.

##### Tests

- `llvm/test/tools/llvm-readobj/ELF/file-header-machine-types.test:78-82` — the shared check
  line at :492 is `# CHECK: Machine: [[MACHINE]]`, unanchored. `"Digital Alpha"` is a prefix
  of `"Digital Alpha (standard)"`, so the `EM_ALPHA` RUN line passes even if readobj printed
  the `EM_ALPHA_STD` name. Only the `EM_ALPHA_STD` line can actually fail. Pick names where
  neither is a prefix of the other (e.g. `"Digital Alpha"` and `"Digital Alpha (gABI 41)"`
  still collide — use something like `"DEC Alpha (EM_ALPHA_STD)"`), or anchor the check.

- `llvm/test/tools/llvm-readobj/ELF/hash-table.test` — the quirk at
  `llvm/tools/llvm-readobj/ELFDumper.cpp:257` now keys on `0x9026`, so an object with
  `e_machine == 41` *starts* getting the unsupported-hash-table warning. That behavior change
  is untested. Also, the surrounding comments at `hash-table.test:56` and `:72` still say
  "on EM_S390 and EM_ALPHA platforms" without noting that `EM_ALPHA` now means only 0x9026.

- No test covers `ELFObjectFile::getFileFormatName()` returning `elf64-alpha` or
  `getArch()` returning `Triple::alpha` (`llvm/include/llvm/Object/ELFObjectFile.h:1355,1461`).
  An `llvm-objdump -f` or `llvm-readobj` test on a yaml2obj `EM_ALPHA` object would cover both.

##### Structure

The `ELFObjectFile.h` hunks depend on `Triple::alpha` from `fae618baf9c5` — ordering is
correct.

---

#### `2fb9f74d8aa4` — [BinaryFormat][Object] Add the Alpha ELF relocations

##### Correctness

Numbers check out against binutils `include/elf/alpha.h`, including the two gaps.

- `llvm/include/llvm/BinaryFormat/ELFRelocs/Alpha.def:6-7`:

  > `// Numbers 12-16 and 20-23 are deprecated ECOFF relocs and are intentionally`
  > `// unused.`

  Only 12-16 are the deprecated ECOFF relocs (`OP_PUSH`, `OP_STORE`, `OP_PSUB`,
  `OP_PRSHIFT`, `GPVALUE`). binutils records 20-23 as *used by Compaq compilers*
  (`IMMED_GP_16` and friends), which is a different reason. The comment conflates them.

- `llvm/lib/Object/RelocationResolver.cpp:255-282` — `supportsAlpha`/`resolveAlpha` are
  **completely untested**. The commit message states the motivation ("so llvm-dwarfdump can
  read the debug sections of an unlinked object") but adds no `llvm-dwarfdump` test, and
  there is no `.def`-driven coverage either. A wrong sign on `SREL32`/`SREL64` would go
  unnoticed. Add a yaml2obj + `llvm-dwarfdump` test, as the sparc64/systemz resolvers have.

- `resolveAlpha` also does not handle `R_ALPHA_NONE`, which `supportsAlpha` correctly
  excludes — consistent, just noting it is deliberate.

##### Tests

- `llvm/test/tools/llvm-readobj/ELF/reloc-types-alpha.test:1-2`:

  > `## Test that llvm-readobj/llvm-readelf shows proper relocation type`
  > `## names and values for the alpha target.`

  Only `llvm-readobj` is run. Every sibling `reloc-types-*.test` runs both. Either add the
  `llvm-readelf` RUN line (with the corresponding `--check-prefix`) or fix the comment.

##### Commit message

"Add the R_ALPHA_* set (0-41 ...)" — the set is not 0-41; nine numbers in that range are
deliberately absent. Say "0-41 with the deprecated and Compaq-only numbers omitted".

---

#### `3caa5a0a3714` — [MC] Widen MCSymbolELF's st_other storage to five bits

##### Correctness

The bit arithmetic is right: `ELF_STV_Shift = 5` (2 bits, 5-6), `ELF_STO_Shift = 7` (now
5 bits, 7-11), flags at 12-15, exactly filling `MCSymbol::NumFlagsBits == 16`.

- `llvm/lib/MC/MCSymbolELF.cpp:47` —

  ```
  static_assert(ELF_IsMemoryTagged_Shift == 15, "ELF symbol flags are full");
  ```

  This asserts a constant against its own literal. It cannot detect the failure the
  preceding comment describes (overflowing `MCSymbol::NumFlagsBits`); it only fires when
  someone edits the line above it and forgets to edit this one. The comment even concedes
  this: `"-- which this cannot check for you, since NumFlagsBits is protected"`. The right
  fix is to make `MCSymbol::NumFlagsBits` accessible (it is at
  `llvm/include/llvm/MC/MCSymbol.h:116`) — e.g. `friend class MCSymbolELF` or promote it —
  and write `static_assert(ELF_IsMemoryTagged_Shift < MCSymbol::NumFlagsBits)`. As written,
  a reviewer will ask for the assert to be deleted or made real.

- `MCSymbolELF.cpp:25-28` — the comment says "bits 0-1 are visibility" but the new
  `assert((Other & 0x7) == 0)` at :164 requires bits 0, 1 **and 2** to be clear. Bit 2 of
  `st_other` is now unrepresentable and the comment does not say so.

- `MCSymbolELF.cpp:26-27` — "Alpha's STO_ALPHA_STD_GPLOAD (0x88) sets bit 3". 0x88 sets bits
  3 *and* 7; bit 7 already round-tripped, bit 3 is the one that was lost. Stating only "bit 3"
  is technically the relevant half but reads as if 0x88 == 0x08.

##### Tests

**None.** `setOther`/`getOther` round-tripping bits 3-7 is exactly the kind of thing
`llvm/unittests/MC/` exists for, and the commit message claims the round trip as its result.
Without a target that emits `st_other` bit 3 (which does not land until much later in the
series), this commit is completely uncovered. Add an `MCSymbolELF` unittest, or move this
commit adjacent to the Alpha MC commit that needs it and give it an `.s` test there.

---

#### `860c777d8c46` — [MC] Skip AsmToken::Comment at statement start in AsmParser

##### Correctness

`AsmToken::Comment` is produced by `AsmLexer.cpp:297`, and `AsmParser.cpp:916` already
loops over `AsmToken::Comment` in the pre-statement path, so skipping it in `parseStatement`
is consistent with existing handling.

- A reviewer will ask why the fix is not in `eatToEndOfStatement()` — the function that
  leaves the stray token. If skipping at statement start is the right layer, the commit
  message should say why (e.g. other raw-lexer paths can leave it too). Right now it just
  describes the symptom.

##### Commit message / comments

- `AsmParser.cpp:1728-1730` — the three-line comment is a verbatim restatement of the
  commit message body. One of the two should be trimmed; the code comment can be one line
  ("eatToEndOfStatement() lexes raw and can leave a block comment as the current token").
- "so they do not confuse start-of-statement logic" — vague. Say what actually goes wrong:
  a comment-only line is taken as the start of a statement and produces a spurious
  `unexpected token at start of statement` error. The test comment gets this right; the
  commit message and the code comment do not.

##### Tests

`llvm/test/MC/AsmParser/block-comment-after-error.s` is a genuine regression test and the
`CHECK-NOT` is correctly scoped after the `:9:10:` match. Two notes:

- The `## ... ##` preamble (lines 4-7) is four lines to explain a two-line test. Two lines
  would do.
- The test only covers a block comment left behind by an *error* path. If the raw lexer can
  strand a comment on non-error paths too, that case is untested — but if it cannot, the
  commit message's general claim ("Block comments can be left as the current token by
  eatToEndOfStatement()") should be narrowed to the error recovery path.

---

#### `42f2bef50b85` — [MC] Increase asm-macro-max-nesting-depth default to 100

##### Correctness / commit message — **the message and the code comment disagree**

Commit message:

> `glibc's sysdep.h macro definitions nest more than 20 levels deep when`
> `expanded through multiple layers of wrappers.`

Code comment at `llvm/lib/MC/MCParser/MCAsmParser.cpp:26-30`:

> `the Linux Alpha port's PALcode and system-call macros reach the low thirties`

These are two different justifications for the same number, in the same commit. Pick one
(preferably the one you can point a reviewer at) and make both places agree.

- Neither explains **100** specifically. If real code reaches "the low thirties", the
  obvious question is why not 64. Say what the headroom is for.
- `MCAsmParser.cpp:24-30` — five lines of comment on a `cl::opt`. The "GNU as has no
  nesting limit at all; this one exists only to turn runaway recursion into a diagnostic
  instead of a stack overflow" sentence is the valuable one; the rest is the commit message
  again.

##### Tests

- `llvm/test/MC/AsmParser/macro-max-depth.s:2` — pinning the limit rather than relying on
  the default is the right call, and the added comment ("which is a tuning knob") is
  appropriate.
- Nothing tests the new default of 100. A RUN line with 30 nested macros and no explicit
  `-asm-macro-max-nesting-depth` would make the change itself testable; as it stands the
  behavior change this commit is *about* has no coverage.

---

#### `61e17ecc3c70` — [clang] Diagnose the _BitInt suffix as an extension in GNU C modes

##### Correctness

The three-way `CPlusPlus / C23 / GNUMode / else` chain is duplicated identically in
`PPExpressions.cpp:340-344` and `SemaExpr.cpp:4054-4058`, which matches the pre-existing
structure. Both sites were updated — good; that duplication is a pre-existing wart, not
this patch's problem.

- The new diagnostic text is `"'_BitInt' suffix for literals is a Clang extension"`, but the
  condition is `getLangOpts().GNUMode`, i.e. this is a *GNU* extension Clang is accepting for
  GCC compatibility. Reusing the C++ wording is defensible (the C++ one is genuinely a Clang
  extension) but a reviewer may prefer distinct wording, since the two now differ in why they
  fire.

- `clang/include/clang/Basic/DiagnosticCommonKinds.td:243-245` — a three-line rationale
  comment above a diagnostic definition. `.td` diagnostic defs in this file carry no such
  comments; the rationale belongs in the commit message, where it already is verbatim.

##### Tests

Good coverage: `gnu` (with `-Wbit-int-extension`) proves the new diag fires,
`gnuquiet` (default flags) proves it is silent by default, and the `__wb`/`__uwb` rows prove
the invalid-suffix path is unaffected. This is the strongest test in the chunk.

- `clang/test/Lexer/bitint-constants-compat.c:4-7` — the added comment is the commit message
  pasted in, including the symmetric "`not as a use of a C23 feature, and not as pre-C23
  incompatibility`" construction. Delete it or reduce it to naming the two new RUN lines.

##### Missing

No `clang/docs/ReleaseNotes.rst` entry. A new user-visible diagnostic (and a behavior change
under `-std=gnu17 -Werror`) needs one; upstream clang reviewers ask for this every time.

---

#### `a847f0253aa4` — [clang] Silently ignore GCC's noclone attribute

##### Correctness — **three contradictory rationales in one commit**

Commit message:

> `Clang has no way to honour it -- the passes that clone (IPSCCP function`
> `specialization, partial inlining, hot/cold splitting) have no per-function`
> `opt-out attribute`

`clang/include/clang/Basic/Attr.td:2427-2431`:

> `noinline already blocks the LLVM passes that would clone a body (function`
> `specialization, partial inlining). A bare noclone is therefore the only case`
> `that loses anything`

`clang/test/Sema/attr-noclone.c:4-6`:

> `Clang does not clone functions, so it is accepted and ignored rather than`
> `warned about`

The third is **simply false** — LLVM does clone functions (IPSCCP function specialization,
partial inlining, hot/cold splitting, and the commit message itself lists them). The first
two are compatible but say different things, and the code comment omits hot/cold splitting
which the message includes. Reconcile these to one statement, and fix the test comment.

##### Structure / naming

- `Attr.td:2426` — the def is named `GCCNoClone` and placed between `NoInline` and
  `NoOutline`. The other `IgnoredAttr`s (`Bounded`, `NvWeak`, `Win64`, the anonymous
  `Declspec<"property">` one) are named after the spelling without a vendor prefix. `NoClone`
  is free; the `GCC` prefix is redundant with `let Spellings = [GCC<"noclone">]`.
- `IgnoredAttr` sets no `Subjects`, so `__attribute__((noclone)) int x;` is silently accepted
  on a variable. That is standard `IgnoredAttr` behavior, so it is fine — but it means the
  test's function-only cases do not pin much.

##### Tests

`clang/test/Sema/attr-noclone.c` — three declarations, all `expected-no-diagnostics`.

- Nothing tests that `noclone` with arguments (`__attribute__((noclone(1)))`) is diagnosed.
  GCC's `noclone` takes none, and `IgnoredAttr` still runs the arg-count check.
- The three cases (plain, alongside `noinline`, on a redeclaration) all exercise the same
  code path — "attribute is ignored". One would do; the redeclaration case is the only one
  that could conceivably differ.

##### Missing

No `clang/docs/ReleaseNotes.rst` entry for a newly accepted GCC attribute.

---

#### `dad7b844f932` — [clang] Suppress -Wuninitialized for unevaluated builtin args

##### Correctness

Replacing the hard-coded builtin IDs with `C->isUnevaluatedBuiltinCall(*Context)` is the
right generalization, and it matches how `SemaChecking.cpp:15254` and
`EvaluatedExprVisitor.h:88` already gate on the same predicate.

- **The commit message understates the scope.** `UnevaluatedArguments` is set on **seven**
  builtins in `clang/include/clang/Basic/Builtins.td`, not four. Beyond the two named in the
  message, this change also newly omits arguments for:
  - `__builtin_os_log_format_buffer_size` (`Builtins.td:5283`) — variadic, and its arguments
    *are* evaluated by the paired `__builtin_os_log_format`;
  - `__GetExceptionInfo` (`Builtins.td:2919`, MS);
  - `__builtin_infer_alloc_token` (`Builtins.td:5111`).

  `OmitArguments` removes the argument sub-expressions from the CFG entirely, so any
  side effects in them stop being modelled for *every* CFG consumer (`-Wunreachable-code`,
  liveness, the static analyzer's `ExprEngine`), not just `-Wuninitialized`. Either list all
  seven in the message and argue each is safe, or restrict the predicate. As written, a
  reviewer will find the extra three on their own and ask.

##### Tests

`clang/test/Sema/warn-uninit-unevaluated-builtin.c` is `// expected-no-diagnostics` with four
functions that must produce nothing.

- **The test passes vacuously** if `-Wuninitialized` regresses or the RUN line's flag is
  dropped. Add a negative control — one function that *does* pass an uninitialized variable
  to a normal call and *does* warn — so the test proves the warning machinery is live.
- The two builtins the commit actually changes behavior for (`__builtin_classify_type`,
  `__builtin_constant_p`) are covered; the two pre-existing ones are regression guards, which
  is fine. None of the three *newly affected* builtins listed above are tested.

##### Commit message

- `"preventing false positives from dataflow analyses like -Wuninitialized"` and
  `"This extends correct treatment to ..."` — "This extends" / "This ensures" phrasing;
  state it directly ("`__builtin_classify_type` and `__builtin_constant_p` also carry
  `UnevaluatedArguments` and were not covered").
- The parenthetical `"(the variables' values are never read; only their types are inspected)"`
  is correct for `classify_type` but not for `constant_p`, which inspects constant-ness, not
  the type.

##### Missing

No `clang/docs/ReleaseNotes.rst` entry for the `-Wuninitialized` behavior change.

### Backend skeleton and first-wave instruction selection (commits 12–40)

#### `5eb60e54dd1a` — Add experimental backend skeleton

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

#### `18e57be6d1aa` — Lower formal arguments and returns

- The shared-slot ABI (`CCAssignToRegWithShadow`) is the substantive change here
  and is **not tested**. `ret.ll` only has all-integer or all-FP signatures,
  which pass identically with the broken independent-list version. The mixed
  test (`call-arg-slots.ll` `@mixed`, `addt $f17, $f19`) lands 12 commits later
  in `5c1f00228c19`; move it here.
- `ret.ll:3-5` restates the commit message as a file header comment; delete.

#### `dba8e92dce87` — Select register-register integer arithmetic

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

#### `8094bb291ca3` — Materialize small signed constants with lda

- `imm.ll:3-5` — "Only immediates that fit in the 16-bit signed displacement are
  handled for now; wider constants are not yet supported." Stale two commits
  later (`46b4919f85ef`); the commit message already says this.

#### `52fd03b365bc` — Select aligned loads and stores

- Operand order is inconsistent: `memri` is `(ops GPRC, s16imm)` (base, disp)
  but `LDA`/`LDAH` are `(ins s16imm:$disp, GPRC:$Rb)` (disp, base), and
  `MFormD` binds `Rc/Rb/disp`. Worth making uniform before the encoder lands.
- `LDQ`/`LDT`/`LDS`/`STQ`/`STT`/`STS` use bare `AlphaInst` + `let Opcode` even
  though `MForm` was added in the previous commit specifically for them.
- **Untested**: `STS` (f32 store) is added but `load-store.ll` has no `sts`
  case. Also untested: a non-zero displacement on a store, and the >16-bit
  offset fallback path in `SelectADDRri`.
- `SelectADDRri` shadows `FIN` in two nested scopes; harmless but noisy.

#### `6fb6f03e71a7` — Select register-register shifts

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

#### `7ce4f183a39f` — Select integer comparisons (setcc)

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

#### `f7a2bf2d6b85` — Select i64 select via conditional move

- `CMOVNE` encoding 0x11/0x26 is correct; the tie-constraint form is right.
- `select.ll` `@seltrunc` checks only `; CHECK: cmovne`. The interesting part of
  that test case is that `trunc i64 %c to i1` must produce an explicit `and`
  with 1 before `cmovne` (which tests the whole 64-bit register, not bit 0).
  The CHECK cannot distinguish correct from miscompiled output — add the `and`.

#### `5363bd96beda` — Select branches

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

#### `53d571d0bf83` — Implement frame lowering

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

#### `46b4919f85ef` — Materialize wider constants with ldah/lda

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

#### `5020677fd0bc` — Select IEEE floating-point arithmetic

- Encodings 0x080/0x0a0/0x081/0x0a1/0x082/0x0a2/0x083/0x0a3 are correct.
- Eight instructions added, five tested: `SUBS`, `MULT`, `DIVS` have no test.

#### `8930ce0fc764` — Select floating-point sign operations

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

#### `c74d01991ef8` — Zero-extend narrow values with zapnot

- `zapnot` 0x12/0x31 correct; masks 1/3/15 correct.
- The commit's rationale (avoids materialising 0xffffffff, which cannot yet be
  built) is genuinely useful and correctly placed.
- Untested: any `and` with a mask that is *not* one of the three, e.g.
  `and i64 %x, 65280` — which at this point cannot be selected at all.

#### `87b5376219bb` — Select immediate-form operate instructions

- `SUBQi`'s pattern `(sub i64:$Ra, immUExt8:$lit)` is effectively dead:
  DAGCombiner canonicalises `sub x, C` into `add x, -C`, so `x - 5` will
  materialise `-5` with `lda` and use `ADDQ` rather than `subq $16, 5, $0`.
  Either add a `(add x, negimm) -> SUBQi` pattern or drop the claim. Nothing in
  `alu-imm.ll` tests `subq` with an immediate, so the gap is invisible.
- `MULQi` is likewise added with no test.
- Timestamps are out of order relative to the neighbour: this commit is
  11:22:59, the next one (`216703c46f12`) is 11:21:00. Cosmetic, but it shows
  the reordering.

#### `216703c46f12` — Sign-extend narrow integers

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

#### `0e6417e3359a` — Select 32-bit loads and stores

- `ldl` 0x28 / `stl` 0x2c correct. Patterns look right.
- No issues found beyond the general `AlphaInst`-instead-of-`MForm` point.

#### `1b9fe58dbb1e` — Select f32/f64 precision conversions

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

#### `7a1b1dc184e9` — Move bits between the integer and floating registers

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

#### `24278df5bb0a` — Convert between integer and floating values

- `(i64 (fp_to_sint f32:$x)) -> (MOVf2i (CVTTQ (CVTST f32:$x)))` inserts a
  `cvtst` that is a no-op for the reason above; `cvttq` can read the register
  directly.
- Four patterns added, three tested: the `fp_to_sint f32` path is untested.
- `int-fp.ll` `@sitofp_f32` checks only `cvtqs $f0, $f0` and skips the stack
  bounce that the header comment advertises.
- `CVTQT`/`CVTQS`/`CVTTQ` again carry no function code (0x0be / 0x0bc / 0x0af).

#### `0e2536386bb5` — Select complement logical operations

- `bic` 0x08, `ornot` 0x28, `eqv` 0x48 all correct; the `not` -> `ornot $31, x`
  pattern is right.
- Untested: the commuted `and (not b), a` form, and the immediate forms
  (`bic $Ra, lit, $Rc`) are not provided, so `and i64 %x, -256` still needs a
  materialised constant.

#### `52acfca806c9` — Select byte/word memory access with BWX

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

#### `ef58a1225d42` — Materialize global addresses via the GOT

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

#### `5c1f00228c19` — Lower direct calls

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

#### `4838f88319d0` — Materialize constant pools GP-relative

- Two changes in one commit: GP-relative `ConstantPool` lowering, and the
  `(f64 (extloadf32 addr)) -> (CVTST (LDS addr))` pattern. The second is a
  separate concern, and is also a pessimization for the same reason as
  `1b9fe58dbb1e`: `lds` already yields the T-format value, so the `cvtst` is
  redundant. `fp-constant.ll` `@add1` asserts `lds; cvtst` as expected output.
- `fp-constant.ll` hardcodes `.LCPI0_0` in `@add1` but uses `{{.*}}` in
  `@addpi`, so the second function does not verify that the two halves reference
  the *same* constant-pool symbol — a wrong-symbol bug would pass.

#### `49fd1690fdb7` — Materialize constants wider than 32 bits from the pool

- **The commit message describes code that is not in the diff** (see Summary
  item 3). Rewrite it to match: the predicate is `!isInt<16>(Hi)`, which covers
  both >32-bit values *and* the 0x8000-high-half range.
- The `LDQ` machine node is built via `getMachineNode` with no chain and no
  `MachineMemOperand`, unlike every other `LDQ` in the backend (which gets both
  from the `load` pattern). Use `MachinePointerInfo::getConstantPool` at
  minimum.
- No test for the boundary case the code's own comment calls out
  (`0x7fff8000`..`0x7fffffff`); `imm64.ll` has a single function.

#### `dce08602d2ed` — Select floating-point comparisons

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

#### `eabc77df741a` — Select floating-point select via fcmovne

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

#### `9ace8008e4c4` — Enable unsigned integer/floating conversions

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

### Stack arguments, atomics, varargs, division, jump tables (commits 41–69)

#### e0867294c3f7 — Pass arguments on the stack

**Correctness**

- `AlphaISelLowering.cpp` `LowerCall`: nothing handles `VA.getLocInfo()`
  (`SExt`/`ZExt`/`AExt`/`BCvt`) or `VA.needsCustom()`; the raw `CLI.OutVals[I]` is
  stored. An i32 stack argument is stored with a full-width `stq` of an unextended
  value. Likewise `LowerFormalArguments` loads `VA.getLocVT()` with no extension
  handling. Only i64 is exercised.
- `StackPtr = DAG.getRegister(Alpha::R30, MVT::i64)` uses the physical SP directly
  rather than `DAG.getCopyFromReg(Chain, DL, Alpha::R30, PtrVT)`. Most targets use the
  latter so the use is chained; a raw `getRegister` is fragile if a frame pointer or
  stack realignment is ever added.
- `MFI.CreateFixedObject(VA.getLocVT().getStoreSize(), ...)` on the callee side: for a
  4-byte slot in an 8-byte-strided area this creates a 4-byte object. Correct
  little-endian, but the object size no longer matches the ABI slot; consider 8.

**Tests** — `stack-args.ll` covers only i64. No f64 stack argument (which goes in the
same slot but through `stt`), no i32, no more-than-one stack argument (offset 8, 16),
which is where an offset bug would show.

**Commit message** — accurate and appropriately scoped.

#### 9b1ed46c35ba — Lower indirect calls

**Correctness** — fine. Removing the `else report_fatal_error` and letting `Callee`
fall through is right.

**Style** — the comment
```
  // Otherwise the callee address is already a value; use it directly as the
  // procedure value for an indirect call.
```
dangles after an `if/else if` chain with no statement attached. Either fold it into the
chain as an explicit `else`-less comment above the `if`, or reword.

**Tests** — `call-indirect.ll` is close to vacuous. `jsr $26, ($27)` and
`ldgp $29, 0($26)` are emitted for *direct* calls too (they are one `AlphaInst`
asm string), so the CHECK block passes without any indirect-call support. The test
must pin the move of the callee into `$27`, e.g. `CHECK: bis $31, $16, $27`.

#### a5407c4342cc — Select atomic loads, stores and fences

**Correctness**

- `MB` is defined with only `let Opcode = 0x18`. Opcode 0x18 with function code 0 is
  `trapb`, not `mb`; the function field `0x4000` is only added much later. Any object
  emission between here and that fix produces `trapb`.
- No `atomic_load_8`/`_16` or `atomic_store_8`/`_16` patterns. `load atomic i8` fails
  to select here; the pre-BWX/BWX byte paths land in later commits. Worth stating in
  the message alongside the RMW caveat that is stated.
- The comment above `setMaxAtomicSizeInBitsSupported(64)` ("Aligned integer loads and
  stores are atomic; barriers are inserted around stronger orderings") does not
  explain the 64, which is the only thing that line does.
- The same explanatory paragraph is duplicated verbatim between
  `AlphaISelLowering.h:56` and the `.td` comment above the atomic patterns. Keep one.

**Structure** — `b9255a7` is a bug fix for this commit and should be squashed in.

#### b9255a744df2 — Fence a sequentially consistent load on both sides

**Correctness** — the fix itself is right and matches PowerPC.

- The comment on `emitTrailingFence` is wrong: "an acquire fence after the access is
  … what breaks the dependent-load problem `shouldInsertFencesForAtomic` describes."
  `emitTrailingFence` only fires for acquire-or-stronger; a *monotonic* load gets no
  trailing fence at all, so it does not address the dependent-load problem for relaxed
  loads. Either drop the second clause or state the actual scope.

**Commit message / structure**

- `Fixes ALPHA-006.` — no such tracker upstream. Remove.
- The paragraph "atomic-load-store.ll checked the missing-barrier sequence, and both
  seq_cst cases now pin the trailing mb too" narrates the diff.
- Squash into `a5407c4`; a seq_cst load is unordered for 3 commits otherwise.

**Duplication** — the 6-line store-buffer explanation appears once in
`AlphaISelLowering.h:63-68`, again in `AlphaISelLowering.cpp:274-277`, again in the
commit message, and a fourth time in `atomic-load-store.ll`. One place.

#### 371b5fa192c9 — Lower atomic RMW with ldq_l/stq_c

**Correctness**

- `shouldExpandAtomicRMWInIR` returns `AtomicExpansionKind::None` unconditionally,
  but only six i64 pseudos exist. `atomicrmw nand`, `min`/`max`, `fadd`, `uinc_wrap`,
  and every i8/i16/i32 form reach isel with no pattern → "Cannot select". `a3cd1ae`
  fixes this three commits later; either land them together or return `CmpXChg` for
  the unhandled cases here.
- `emitAtomicRMW` does not transfer `MI`'s `MachineMemOperand`s to the new `LDQ_L`
  and `STQ_C`. The generated loads/stores have no MMO, which loses volatility and
  ordering information and makes post-RA scheduling/AA conservative-by-accident rather
  than by construction. Same omission in `emitAtomicCmpXchg`, `emitSafeStore`,
  `emitSubwordAtomicRMW` and `emitSubwordCmpXchg`.
- The loop is built pre-RA with virtual registers, so the register allocator may
  insert a spill or reload between `LDQ_L` and `STQ_C`, clearing the lock flag and
  live-locking. PowerPC does the same thing so it is defensible, but RISC-V
  deliberately expands post-RA. Worth a comment saying which hazard was accepted.
- `LDQ_L`/`STQ_C` are bare `AlphaInst` with only `Opcode`; `MForm` already exists in
  `AlphaInstrFormats.td` at this point and is used later. Use it here.

**Tests** — `atomic-rmw.ll`:
- `and` checks no `stq_c` and no `beq`, so a broken store-conditional would pass.
- `xchg`'s `; CHECK: beq` with no operand matches any branch.
- No seq_cst/acquire/release RMW test, so the interaction between
  `shouldInsertFencesForAtomic` and `AtomicExpansionKind::None` (does AtomicExpand
  still hoist the fences?) is untested here. `a3cd1ae` adds
  `atomic-rmw-minmax-ordering.ll` for the CmpXChg path but never for this path.

**Formatting** — the opcode `switch` uses aligned multi-statement lines
(`case Alpha::ATOMIC_ADD_I64:  Opc = Alpha::ADDQ; break;`); clang-format will not
produce that.

#### a3cd1ae11ccf — Lower atomic compare-and-swap

**Correctness**

- **The `atomic_cmp_swap` fragment is unconstrained** (`AlphaInstrInfo.td:529`):
  ```
  [(set i64:$dst, (atomic_cmp_swap iPTR:$addr, i64:$cmp, i64:$new))]
  ```
  compare with the RMW pseudos two lines up, which correctly wrap in
  `AtomicI64<>` with `IsAtomic`/`MemoryVT`. An i8/i16/i32 `cmpxchg` (type-legalized to
  an i64-valued node with a narrow MemoryVT) matches this pattern and gets a 64-bit
  `ldq_l/stq_c` on the raw, possibly unaligned, address. See Summary item 1.
- The commit message asserts "The cmpxchg the expander builds is the form lowered
  here, sub-word included, so the sub-word min/max forms work too." That is false at
  this point in the series — sub-word cmpxchg lowering does not exist until
  `ebda925`, twenty commits later. Right now it "works" only because of the missing
  `MemoryVT` guard, i.e. it silently miscompiles.

**Structure** — two concerns in one commit: (a) the i64 cmpxchg inserter, (b)
redirecting the remaining RMW kinds to `AtomicExpansionKind::CmpXChg`. Split, or at
least merge (b) back into `371b5fa` where the gap was introduced.

**Commit message** — the last paragraph (sub-word unsigned compare on sign-extended
operands, verified for every pairing of 0/1/127/128/129/254/255) describes behavior
that does not exist in this commit. Move it to `ebda925`.

**Tests**

- `atomic-cmpxchg.ll` never checks the success flag, only `extractvalue …, 0`.
- `atomic-rmw-minmax-ordering.ll` is the strongest test in the chunk — the
  `CHECK-NOT: mb` bracketing is the right way to assert "exactly these barriers".
  Note that the `minmax_monotonic` case is `nand`, and `minmax_acquire` is `umax`
  while `minmax_seq_cst`/`minmax_release` are `min`; the function names then do not
  describe what is tested. Use one operation throughout or rename.

#### 27dc8ed9d1db — Synthesize pre-BWX byte and word loads

**Correctness**

- `EXTBL` and `EXTWL` both carry `let Opcode = 0x12` and nothing else, so they encode
  to the same 32 bits (and with no register fields). Correct is
  `OForm<0x12, 0x06>` / `OForm<0x12, 0x16>`, which is what the tree ends up with.
- `SelectAddrReg` returns the address node unchanged, including a bare `FrameIndex`.
  Until `bf53c30` (eleven commits later) nothing selects `ISD::FrameIndex`, so
  `load i8, ptr %alloca` on a pre-BWX target hits the "cannot select" path. `bf53c30`
  belongs before this commit.
- Correctness of `extwl` for a *word* depends on the word not straddling a quadword
  boundary. That holds only because `allowsMisalignedMemoryAccesses` is never
  overridden (so LegalizeDAG splits under-aligned i16 accesses). That is load-bearing
  and undocumented; add a comment, and ideally an `align 1` test.

**Tests** — `prebwx-load.ll` has no unaligned (`align 1`) i16 case, which is the case
the correctness argument rests on.

#### 4ea758cf6a00 — Synthesize pre-BWX byte and word stores

**Correctness** — the pattern-based sequence introduced here is the one `6328b37`
identifies as losing stores. It also introduces four instructions (`INSBL`, `INSWL`,
`MSKBL`, `MSKWL`) that all encode as bare opcode `0x12`.

**Structure** — squash with `6328b37`. Do not land a known data-loss miscompile as an
intermediate state; the surviving commit message can keep `6328b37`'s excellent
explanation of *why* the pseudo is needed.

**Tests** — the header comment ("Checking a particular order here asserts a scheduling
decision … and pins whichever order the compiler happened to produce on the day") is
good rationale but reads as prose essay; two sentences would do.

#### 6328b37999b80 — Keep the pre-BWX byte/word store together

**Correctness**

- `AlphaISelDAGToDAG.cpp` `Select()`: the new MMO is built from
  `MachinePointerInfo()` with `Align(8)` and only `MOVolatile` carried over. The
  original `MachinePointerInfo`, AA metadata, and other flags (`MONonTemporal`,
  `MOInvariant`) are dropped. Prefer deriving from `ST->getMemOperand()` via
  `MF.getMachineMemOperand(ST->getMemOperand(), ...)`.
- `BICi` is defined here but never used by this commit — the expansion relies on
  `ldq_u` ignoring the low three address bits. Its only consumer is `emitSafeStore` in
  the *next* commit. Move the definition there.
- `BICi` is another bare `AlphaInst` with `Opcode = 0x11` and no function field; `bic`
  needs `OFormL<0x11, 0x08>`.
- The alignment guard `ST->getAlign() >= MemVT.getStoreSize()` is only correct because
  under-aligned i16 stores are split by LegalizeDAG (see `27dc8ed`); if that ever
  changes, an unaligned i16 silently falls through to no pattern at all, since the
  `truncstorei16` pattern was deleted in this commit. A `default: llvm_unreachable`
  style comment, or an explicit `align 1` test, would pin it.

**Commit message** — the best in the chunk: it says what broke, how it manifested
(ASan default flags), and why the fix is shaped as it is. Keep this text when squashing.

#### 663dac63231e — safe-bwa feature

**Correctness** — the `ldq_l/stq_c` sequence itself is right, and hoisting the `bic`
and the `insXl` out of the loop is correct since neither depends on the loaded value.

- `SAFE_STOREI16`'s pattern `(truncstorei16 GPRC:$val, ADDRr:$addr)` has no alignment
  guard, unlike the `RMW_STOREI16` path in `AlphaISelDAGToDAG.cpp`, which requires
  `getAlign() >= 2`. The two paths should be guarded identically even if legalization
  makes both safe today.
- Feature name: consider `+safe-bwa` matching gcc is fine, but the LLVM convention is
  to describe the behavior; `FeatureSafeBWA`'s description string is good.

**Tests** — `prebwx-store-safe.ll` does not test that `+bwx,+safe-bwa` still emits a
plain `stb` (the interaction of the `SafeBWStore` predicate with the DAGToDAG guard is
the only thing keeping that from double-selecting).

#### a299923e57e1 — 32-bit atomic RMW

**Correctness**

- The comment above `AtomicRMW` is edited to read "expanded to an ldl_l/ldq_l retry
  loop" — the store-conditional half is what distinguishes the two.
- i32 add/sub use `ADDQ`/`SUBQ`, not `ADDL`/`SUBL`. Harmless (only the low 32 bits are
  stored, and the returned value is the sign-extended `ldl_l` result), but worth one
  line of comment saying so, since the naive reading is that it is a bug.
- `LDL_L`/`STL_C` are again bare `AlphaInst`.
- `ATOMIC_CMPXCHG_I64` is still unconstrained after this commit, so i32 `cmpxchg` now
  sits next to a correct i32 RMW path while silently using the 64-bit CAS loop.

**Tests** — `atomic-rmw-i32.ll` covers add and xchg only; `; CHECK: beq` in `xchg` is
operand-less.

#### bf53c3077575 — Select a frame index through lda

**Correctness**

- `def LEA : MForm<0x08, (outs GPRC:$Rc), …>` — `MForm` declares `bits<5> Ra`, and
  nothing binds it, because the operand is named `$Rc`. The field is left unset (the
  tree later renames it to `$Ra`). At this commit the encoding is wrong.
- `LEA` and `LDA` both use opcode `0x08` and print `lda`. Without `isCodeGenOnly = 1`
  on one of them the asm matcher has two ways to spell the same instruction.

**Structure** — this is a foundational fix ("any function that takes the address of a
stack slot fails to select"), yet it lands at position 52, after stack arguments
(#41), pre-BWX loads (#47) and varargs. It should be moved to before the first commit
that creates a frame index. The current ordering means a large stretch of the series
cannot compile ordinary code — the exact failure mode recorded in
`alpha-series-needs-intermediate-gates`.

**Tests** — none. A commit that fixes "any function that takes the address of a stack
slot fails to select" must add a test taking `&local`.

#### 408a37e223b5 — Support variadic functions

**Correctness**

- **va_list offset width** — see Summary item 3. `LowerVASTART` stores an i64 at
  `va_list+8`; `LowerVAARG` and `LowerVACOPY` load i64 from `va_list+8`. gcc's Alpha
  va_list is `{ char *__base; int __offset; }`. Reading 8 bytes where gcc wrote 4
  picks up undefined padding.
- `LowerVAARG` for a floating-point `VT`: the register save area holds f64 (`stt`),
  but the load uses `VT` directly, so `va_arg(ap, float)` reads 4 bytes of an f64.
  Invalid C, but reachable from hand-written IR; either assert or handle.
- No handling of arguments requiring more than 8 bytes or greater-than-8 alignment.
- `MFI.CreateFixedObject(48, -48)` / `(48, -96)` relies on PEI extending the local
  frame below the lowest negative fixed object. Correct, but the reasoning belongs in
  a comment because it is the non-obvious part of the layout.

**Formatting** — several lines exceed 80 columns, e.g.
```
      SDValue IntPtr = DAG.getMemBasePlusOffset(IntBase, TypeSize::getFixed(I * 8), DL);
```
and
```
  SDValue Base = DAG.getLoad(MVT::i64, DL, Chain, VAList, MachinePointerInfo(SV));
```

**Tests** — `vararg.ll`:
- `f` checks `stq $21` and `stq $20` but not `$17`–`$19`, and checks five `stt`s but
  not that `$f16` is *not* saved (which is the interesting part of the `NumNamed`
  logic).
- No test of an FP `va_arg` (the `base-48` select in `LowerVAARG` is entirely
  untested).
- No `va_arg` of a type smaller than 8 bytes.
- The `many` test is excellent — it pins the exact frame offsets and explains the
  double-counting bug it guards. Keep the reasoning, halve the prose.

#### 23afada07d4b — Sub-word atomic RMW

**Correctness**

- The result `Dst` is `extbl`/`extwl` output, i.e. zero-extended. That is fine here
  because `getExtendForAtomicOps()` is still the default `ANY_EXTEND`, but commit
  `ee52db35` later changes it to `SIGN_EXTEND`. If that commit does not also introduce
  the `emitSignExtendField` call, everything between is a miscompile — verify the
  gating when reordering.
- No MMO transferred (see `371b5fa`).

**Formatting** — the twelve `case` lines in `EmitInstrWithCustomInserter` are
single-line, aligned, and up to 92 columns:
```
  case Alpha::ATOMIC_ADD_I8:   return emitSubwordAtomicRMW(MI, MBB, Alpha::ADDQ, false);
```
The tree at HEAD has these reformatted, so this is purely a per-commit hygiene issue.

**Tests** — `atomic-rmw-subword.ll` covers `add i8` and `xchg i16` only; no i16 ALU
op, no `and`/`or`/`xor`, and no ordering test. The `xchg16` comment claims the
extract is "hoisted out of the loop" when the CHECK order shows it *after* the loop
(sunk); the code is still correct, but the comment describes the opposite
transformation.

#### 76148385ea06 — Fold gprellow into constant-pool loads

**Correctness** — the fold is right and matches gcc.

- `LDLg`/`LDQg`/`LDTg`/`LDSg` are bare `AlphaInst` with only `Opcode`, so the
  `$Ra`/`$sym`/`$base` operands are not encoded.
- Lines exceed 80 columns:
  `                     "ldl $Ra, $sym($base)\t\t!gprellow", []> { let Opcode = 0x28; }`
- `AddedComplexity = 20` is unexplained; a one-line comment saying it must beat the
  plain `(load (AlphaGprelLo …))` + `LDAg` pairing would help.

**Tests** — `LDLg` (the `sextloadi32` pattern) is never exercised; there is no i32
constant-pool entry in the tests.

#### a9f499b0f67c — Division and remainder via millicode

**Correctness**

- `LowerDivRem` roots the sequence at `DAG.getEntryNode()` rather than at any existing
  chain, and does not use `CALLSEQ_START`/`CALLSEQ_END`. Two divisions in the same
  block therefore produce two independent `CopyToReg($24)/CopyToReg($25)/DIVCALL`
  chains with no ordering between them; correctness relies entirely on the scheduler's
  physical-register interference handling. Every in-tree target that does this kind of
  ad-hoc call (e.g. ARM's `__aeabi` helpers) still wraps it in call sequence markers.
  At minimum add a test with two divisions and one with a division inside a loop.
- `Defs = [R23, R24, R25, R27, R28]` matches gcc's clobber set for the ELF divide
  routines. Good. `Uses = [R24, R25, R27]` likewise.
- `i32 division is promoted to i64` is asserted in the message but untested. `udiv
  i32` in particular needs the zero-extension to be right.

**Formatting** — the `switch` again uses aligned multi-statement lines.

**Tests** — `divrem.ll` checks the register setup only for `sdiv`; the other three
check the symbol and `jsr $23` only, which is reasonable. Missing: two-divisions,
i32 division.

#### 934b84f6f582 — Scaled add/subtract

**Correctness** — function codes `0x22`/`0x32`/`0x2b`/`0x3b` are correct for
S4ADDQ/S8ADDQ/S4SUBQ/S8SUBQ, and the `sub` operand order in the patterns
(`(sub (shl Ra, N), Rb)` → `sNsubq Ra, Rb, Rc`) is right.

Nothing to flag. Missing (fine as follow-up): the longword forms `s4addl`/`s8addl`.

#### fa7763352c1b — Jump tables

**Correctness**

- **Stale comment contradicting the code.** `LowerBR_JT`:
  ```
  // br_jt: chain, jump-table address, index.  Each table entry is a 32-bit
  // GP-relative offset, so the branch target is $gp + table[index].
  ```
  Nine lines later: "Each entry is an absolute 64-bit block address; load it and
  jump." The first comment is left over from a different design; delete it.
- `EK_BlockAddress` puts absolute addresses in the jump table. For a
  position-independent object that requires `R_ALPHA_REFQUAD` dynamic relocations and
  the table to live in `.data.rel.ro`. There is no `getSectionForJumpTable` override
  and no `isPositionIndependent()` check, yet the rest of the target loads globals
  through the GOT (`!literal`), i.e. is PIC-shaped. Either justify the choice in the
  commit message or use a GP-relative offset table.
- `LowerJumpTable` and `LowerBR_JT` compute the same GP-relative address; since
  `BR_JT` is `Custom` and re-derives the address from `Op.getOperand(1)`,
  `ISD::JumpTable` may never be reached. If it is dead, drop it; if not, have
  `LowerBR_JT` call it.
- `JMP` is a bare `AlphaInst` with only `Opcode = 0x1a`; `MbrForm` already exists and
  encodes the sub-opcode and hint. `jmp` needs sub-opcode `0b00`.

**Tests** — `jumptable.ll` is good (it pins the table contents as well as the
dispatch). No test of the default/out-of-range path or of a jump table in a function
that also uses `$gp` for something else.

#### ad087baf804f — Small-data GP-relative addressing

**Correctness** — the lowering is straightforward and correct for the non-preemptible
case.

**Structure / process** — the commit message says outright that the feature is wrong
for preemptible symbols and lacks the `-G` threshold, and that "both are fixed later".
Landing a knowingly incorrect lowering, off by default, and repairing it dozens of
commits later is exactly the kind of thing upstream review rejects. Either squash the
later fixes in or reduce this commit to the feature flag plumbing with the correctness
checks already present.

**Tests** — `small-data.ll` covers both large- and small-data; good. No test of a
preemptible (`external`, default visibility) symbol, which is precisely the known-broken
case — a `TODO`-style negative test would at least document it.

#### 8ab7cce666ee — Fold gprellow into small-data loads/stores

**Correctness** — opcodes `0x2c`/`0x2d`/`0x27`/`0x26` for `stl`/`stq`/`stt`/`sts` are
correct. The `GprelFold` multiclass is a clean refactor and the right shape.

- The `ST*g` defs are bare `AlphaInst` again.
- No fold for i8/i16 (BWX `ldbu`/`stb`), so small-data byte access keeps the extra
  `lda`. Fine as a follow-up, but the commit message's "a small-data global load or
  store is a two-instruction sequence" is broader than what is implemented.

**Tests** — only i64 load and store are checked. The f32/f64 store folds
(`STTg`/`STSg`) and the `truncstorei32` fold (`STLg`) are added but untested.

#### 2a9ca1528b63 — CIX count instructions

**Correctness**

- `def CTPOP : OForm<0x1c, 0x30, …> { let Opcode = 0x1c; }` — the `let Opcode` is
  redundant (already the `OForm` parameter), and more importantly `OForm`'s `Ra` field
  is never bound, since the `ins` list only has `$Rb`. The architecture requires
  `Ra = 31`; an unset field encodes as 0, i.e. `$0`. The tree adds `Ra = 31` later.
  Same for `CTLZ`/`CTTZ`.
- Function codes `0x30`/`0x32`/`0x33` are correct.

**Tests** — `cix-count.ll` is thorough (32-bit variants, parity). Minor
inconsistency: the i64 intrinsics are declared, the i32 ones are not (the IR parser
auto-declares intrinsics, so it works, but be consistent).

#### f54fd8aafd73 — FIX square root

**Correctness**

- `SQRTS` and `SQRTT` are both `AlphaInst` with only `let Opcode = 0x14` — identical
  encodings, and no `$Fa = 31`.
- The commit message says "otherwise it becomes a libcall", but runtime libcalls are
  not enabled until `be8f706` (six commits later), so on a non-FIX subtarget `fsqrt`
  at this point reports "no libcall available". Either reorder `be8f706` before this,
  or drop the claim.

**Tests** — only the `-mcpu=ev6` path. Add the non-FIX run line once `be8f706` is in
place (or after reordering).

#### adb39a8bf310 — Expand bswap

**Correctness** — fine. `ISD::BSWAP` defaults to Legal, so the line is needed.

**Tests** — `bswap.ll` is close to vacuous:
```
; CHECK-NOT:   jsr
; CHECK:       ret
```
A completely wrong expansion (or the identity) passes. Check for the actual sequence
(`zapnot`/`sll`/`srl`/`bis`), or at least a fixed instruction count. The comment
"Check that it selects (rather than failing) and needs no libcall" is honest about the
test being a smoke test, which is a reason to strengthen it, not to keep it.

#### ebda925bb7f8 — Sub-word compare-and-swap

**Correctness**

- **Success flag** — see Summary item 2. `Dst` is zero-extended by `extbl`/`extwl`;
  the outer `SETCC` for `ATOMIC_CMP_SWAP_WITH_SUCCESS` compares it with the promoted
  `$cmp` operand, which is not normalized because `getExtendForAtomicCmpSwapArg()` is
  not overridden. `cmpxchg ptr %p, i8 -1, i8 %n` succeeds but reports failure. Fix
  either by overriding `getExtendForAtomicCmpSwapArg()` to `ZERO_EXTEND` or by
  normalizing `Dst` and `Cmp` the same way (which is what HEAD ends up doing, via
  `emitSignExtendField`).
- **`ATOMIC_CMPXCHG_I32` is missing.** This commit converts the CAS pseudo to
  `MemoryVT`-constrained fragments and defines I64/I16/I8 only, so `cmpxchg i32` goes
  from silently-wrong to "Cannot select". Add I32 here.
- The `ZAPNOTi` of `$cmp` is correct for the in-loop comparison.

**Tests** — `atomic-cmpxchg-subword.ll` extracts the success flag but only checks
instruction shape, so it cannot detect the bug above. Add an execution-semantics-style
test (negative expected value) or at least a CHECK on the final `cmpeq`/`zapnot`
pairing. `; CHECK: cmpeq {{.*}}[[C]]` is loose enough to match almost anything.

#### a839b7e69896 — umulh and expanded signed multiply-high

**Correctness** — `UMULH` opcode `0x13` func `0x30` is correct. `MULHS`,
`SMUL_LOHI`, `UMUL_LOHI`, `ROTL`, `ROTR` all default to Legal in
`TargetLoweringBase::initActions`, so each `Expand` here is load-bearing. Good.

**Structure** — the rotate expansion is unrelated to multiply-high; either split it out
or retitle the commit. The message currently buries it in a subordinate clause.

**Tests** — `mulhs`'s `CHECK: umulh` / `CHECK: ret` does not verify the sign
correction, which is the whole content of the expansion. `divconst`'s
`CHECK-NOT: __divqu` is a good negative check.

#### be8f706f9838 — Runtime libcalls and expanded FP operations

**Correctness**

- Most of the nested loop is redundant. `TargetLoweringBase::initActions` already
  defaults these to Expand for every VT: `FTAN`, `FEXP`, `FEXP2`, `FEXP10`, `FLOG`,
  `FLOG2`, `FLOG10`, `FFLOOR`, `FNEARBYINT`, `FCEIL`, `FRINT`, `FTRUNC`,
  `FROUNDEVEN`, `FMINNUM`, `FMAXNUM`, `FMINIMUM`, `FMAXIMUM`, and (in the "library
  functions" group) `FROUND`, `FPOWI`, `FSINCOS`. Only `FSIN`, `FCOS`, `FPOW`,
  `FMA` and `FREM` actually need setting. Twenty of twenty-four entries are no-ops;
  trim the list so a reader can tell which ones matter.
- `AlphaSystemLibrary` is inserted in `RuntimeLibcalls.td` between the RISCV and SPARC
  blocks. Place it where the file's ordering convention says (the other entries are
  not alphabetical either, but Alpha-after-RISCV looks accidental).

**Structure** — mixes a change to a core LLVM `.td` with a target change. That is
unavoidable here, but call it out in the message so the core change gets reviewed.

**Tests** — `fp-libcalls.ll` is good. The `fmax_f64` case added to
`atomic-rmw-fp.ll` belongs in this commit (it does), fine.

#### f7232f0cd9a9 — Half precision via conversion libcalls

**Correctness** — the four `setOperationAction` calls plus the extload/truncstore
actions are the standard recipe; no issues.

**Commit message** — the only commit in the chunk without the "Verified under
qemu-alpha" tail, which is the right choice; apply it to the rest.

**Tests** — `half.ll` covers extend and truncate. Missing: a bare `half` load/store
with no conversion (memcpy-style), and `fpext half to double` (the f64 actions set
here are untested).

#### 08ff50b405dc — BWX sextb/sextw

**Correctness**

- `SEXTB`/`SEXTW` use `OForm` with only `$Rb` in `ins`, so the `Ra` field is unbound
  and encodes as `$0` instead of the architecturally required `$31`. HEAD adds
  `Ra = 31` to the enclosing `let`; do it here.
- Function codes `0x00`/`0x01` under opcode `0x1c` are correct.

**Tests** — `sextbw.ll` covers both subtargets and the `bwx-mem.ll` update is right.
Good commit overall.

#### 8f7267359a64 — Condition-specific conditional moves

**Correctness**

- **`class CMOV_cc<bits<7> func, …>` never uses `func`.** The class derives from
  `AlphaInst` and sets only `let Opcode = 0x11`, so the function-code parameter is
  silently discarded and `CMOVEQ`, `CMOVLT` and `CMOVGT` all encode identically (and
  identically to `bis`, `and`, `xor`, …). HEAD derives from `OForm<0x11, func, …>`,
  which is what this should have been from the start. This is the most clear-cut
  TableGen bug in the chunk — an unused template parameter that looks used.
- The function codes themselves (`0x24`, `0x44`, `0x66`) are correct.
- No `AddedComplexity`; relies on TableGen preferring the more complex pattern over
  `CMOVNE`'s `(select i64:$Ra, …)`. That happens to be true but is worth a comment.

**Commit message** — the second paragraph explaining why `cmovle`/`cmovge` are omitted
is exactly the kind of *why* an LLVM message should carry. Good.

**Tests** — `cmov-cc.ll`: `sgt` omits the `CHECK-NOT: cmp` that the other two have, so
it would pass with a `cmplt`+`cmovne` pair. Add it. There is a stray trailing blank
line at the end of the file. No test for the FP-condition or the literal-operand
variants.

---

#### Cross-cutting recommendations

**Squash / reorder**

- `4ea758c` + `6328b37` → one commit (known data-loss intermediate).
- `a5407c4` + `b9255a7` → one commit (unfenced seq_cst load intermediate).
- Move `bf53c30` (frame index → `lda`) before `e0867294`; nothing that creates a frame
  index should land first.
- Move `be8f706` (runtime libcalls) before `f54fd8a` so the non-FIX `fsqrt` claim holds.
- Fold the `MemoryVT` constraint and `ATOMIC_CMPXCHG_I32` from `ebda925`/`ee52db35`
  back into `a3cd1ae` so no commit in the range miscompiles or fails on `cmpxchg i32`.
- Split `a3cd1ae` into (a) i64 CAS inserter, (b) `CmpXChg` expansion for the remaining
  RMW kinds — and move (b) into `371b5fa`, which is where the gap appears.
- Split the rotate expansion out of `a839b7e`.
- Introduce each instruction with its real encoding (`OForm`/`MForm`/`MbrForm` and the
  required `Ra = 31` / function code) in the commit that adds it, rather than
  back-filling encodings later.

**LLM tells to clean up**

- The "Verified end to end under qemu-alpha…" sentence on 15 of 29 commits.
- `Fixes ALPHA-006.` (b9255a7) — a private tracker id.
- Commit messages that narrate the test diff rather than the change (b9255a7).
- Duplicated multi-paragraph rationale in header comment + .cpp comment + commit
  message + test header (b9255a7, 6328b37, 4ea758c).
- Test-file preambles that read as tutorials rather than as CHECK rationale
  (`store-byte-rmw.ll`, `prebwx-store.ll`, `atomic-rmw-minmax.ll`) — keep the
  reasoning that justifies a non-obvious CHECK, cut the rest.
- Comments restating the code: `// *va_list = base`, `// va_list[8] = offset`,
  `// Advance the offset by one 8-byte slot.`, `// No byte-swap instruction; expand to
  shift/mask.`
- Generic test function names (`f`, `add`, `xchg`, `and`, `min`, `max`, `cas`) —
  several files reuse the same name across files, which makes failures hard to locate.

**Test gaps worth closing before posting**

- `cmpxchg i32` (any width) — currently untested anywhere in the chunk.
- Ordering (seq_cst/acquire/release) for the *native* RMW path and for the sub-word
  paths; only the CmpXChg-expanded path has `atomic-rmw-minmax-ordering.ll`.
- Two divisions in one function (`LowerDivRem` chaining).
- FP `va_arg` (the `base-48` select is entirely untested).
- A function taking the address of a stack slot (`bf53c30` has no test at all).
- Unaligned (`align 1`) i16 load and store on a pre-BWX subtarget.

### MC layer, disassembler, assembly parser, clang target and ABI, TLS (commits 70–100)

#### 5e5e91b7aafb [Alpha] Add instruction encoding formats (operate, FP, lda)

**Commit message is wrong about what the commit does.** It says:

> Add the OForm/OFormL/FForm/MForm/MFormD/BForm/MbrForm format classes that lay
> out the instruction fields

This commit touches exactly one file, `AlphaInstrInfo.td`
(`1 file changed, 127 insertions(+), 105 deletions(-)`). Those classes were
added by `dba8e92dce87` ("Select register-register integer arithmetic"), well
before this chunk. Rewrite the message to describe what it actually does:
populate `Inst` bits and add the typed symbol operands.

**"No functional change" is false — it silently fixes five wrong function
codes.** `AlphaInstrInfo.td`:

```
-def CMPEQ  : CMP_rr<0x10, "cmpeq">;
-def CMPLT  : CMP_rr<0x10, "cmplt">;
-def CMPLE  : CMP_rr<0x10, "cmple">;
-def CMPULT : CMP_rr<0x10, "cmpult">;
-def CMPULE : CMP_rr<0x10, "cmpule">;
+def CMPEQ  : CMP_rr<0x2d, "cmpeq">;
...
```

The new values (10.2D, 10.4D, 10.6D, 10.1D, 10.3D) are correct; the old ones
were all 0x10 for five different instructions, which is plainly a
copy-paste bug in whichever earlier commit added `CMP_rr`. Either squash the
correction into that commit or call it out in the message — do not hide a
five-instruction encoding fix under "No functional change to assembly output".

**Dead operand introduced with a false rationale.**

```
+// Retained alias so existing definitions read naturally.
+def symbol : SymOperand<"getGprelLowEncoding">;
```

By the *next* commit (`35377a6ad64a`) every `symbol:$sym` use is gone; from
that point `def symbol` is dead and the comment is untrue. Drop it here, or
drop the users first and never add the alias.

Encodings verified correct in this commit: CPYS 17.020, CPYSN 17.021,
FCMOVNE 17.02B, CVTST 16.2AC, CVTTS 16.0AC, CVTQT 16.0BE, CVTQS 16.0BC,
CVTTQ/C 16.02F, SQRTS 14.08B, SQRTT 14.0AB, ADD/SUB/MUL/DIV S/T 16.08x/0Ax,
CMPTEQ/LT/LE 16.0A5/6/7, SEXTB/W 1C.00/01, CTPOP/CTLZ/CTTZ 1C.30/32/33,
ZAPNOT 12.31, S4/S8 ADDQ/SUBQ 10.22/32/2B/3B, BIC/ORNOT/EQV/BIS/CMOVNE
11.08/28/48/20/26, ADDL 10.00, LDA 08, LDAH 09.
`FCpys` (`fa = Rb`, `fb = Ra` for `cpys $Rb, $Ra, $Rc`) correctly implements
`fcopysign(magnitude=Ra, sign=Rb)`.

#### 35377a6ad64a [Alpha] Encode memory, branch and jsr-format instructions

Encodings all verified correct: MB 18.4000, EXTBL/EXTWL 12.06/16,
INSBL/INSWL 12.0B/1B, MSKBL/MSKWL 12.02/12, LDQ_U 0B, STQ_U 0F, LDBU 0A,
LDWU 0C, STB 0E, STW 0D, LDL/LDQ/LDL_L/LDQ_L/STL/STQ/STL_C/STQ_C 28-2F,
LDS/LDT/STS/STT 22/23/26/27, BR 30, BEQ 39, BNE 3D, jsr-format sub-opcodes
jmp=00 / jsr=01 / ret=10, `ret` hint = 1, `jmp` hint = 0, BICi 11.08.

**Duplicated comment.** Above `LDQ_U` the existing comment already says
"ldq_u loads the aligned quadword containing the datum (ignoring the low three
address bits)"; this commit adds directly beneath it:

```
+// ldq_u/stq_u take a base register with an implicit zero displacement; the CPU
+// ignores the low three address bits.
```

The second half restates the first comment verbatim in other words. Keep the
non-redundant half ("implicit zero displacement") and delete the rest.

**Naming inconsistency worth fixing while the file is young.** `MFormD` calls
its 25-21 register field `Rc` while `MForm` calls the same architectural field
`Ra`, so `LDQl`/`LDAg`/`LDLg`... use `$Rc` for a destination that the handbook
calls Ra, and `STLg`/`STQg` use `$Rc` for a *store source*. This is confusing
for anyone cross-checking against the handbook.

**`LDQ_U`/`STQ_U` are the only instructions left writing raw `Inst{}` bits**
instead of using a format class, immediately after a commit whose whole point
is format classes. A trivial `MFormZ` (or reuse of `MFormD` with `disp = 0`)
would remove the outlier.

#### 36370f848867 [Alpha] Add MC code emitter, asm backend and ELF object writer

Verified: `writeNopData` writes `0x47ff041f`, which is exactly
`bis $31, $31, $31`. `elf-obj.ll`'s expected bytes are correct
(`addq $16,$17,$0` = 0x42110400, `ret` = 0x6BFA8001, `sll $16,3,$0` =
0x4A007720). Relocation numbers match binutils.

**Unused member.** `AlphaMCCodeEmitter` stores `MCInstrInfo const &MCII` and
never reads it (the TableGen'd `getBinaryCodeForInstr` does not use it either).
`-Wunused-private-field` territory; drop it or drop the constructor parameter.

**`getBranchTargetEncoding` returns a raw immediate.**

```
  if (MO.isImm())
    return static_cast<unsigned>(MO.getImm());
```

Everywhere else a branch displacement is in instruction units relative to
PC+4; returning the immediate unshifted means an assembled `bne $1, 8` would
encode 8 instructions, not 2. Unreachable today (no asm parser yet), but it
becomes reachable in `c0680590ad5b` and is still wrong there.

**Vacuous claim in the test comment.** `llvm/test/CodeGen/Alpha/elf-obj.ll`:

```
; The .text contents are addq/ret (add) followed by sll/ret (sh); local
; branches, if any, resolve without relocations.
```

There are no branches in this test. Delete the second clause (it is covered by
`branch-obj.ll` two commits later).

**Untested paths**: `adjustFixupValue`'s braddr arithmetic and
`writeNopData` have no test in this commit; `braddr` gets one in
`68b1b88ea461`, `writeNopData` never does.

#### d00ec074d435 [Alpha] Emit relocations and expand ldgp for object output

Correct: `CB.size()` really is 0 at the start of each instruction —
`MCObjectStreamer::emitInstToData` passes a fresh `SmallString<16> Content`
and adds `CodeOffset` afterwards (`MCObjectStreamer.cpp:442-457`), so the
`LdahOffset` trick is sound.

**The GPDISP addend is the whole point of the commit and is untested.**
`llvm/test/CodeGen/Alpha/elf-reloc.ll` only has:

```
; CHECK: R_ALPHA_GPDISP
```

Nothing checks that the addend is 4 (the byte distance to the matching `lda`),
which is precisely what the linker uses to find the second half of the pair. A
wrong addend passes this test and silently produces a broken object. Use
`llvm-readobj -r` output with the addend, e.g.
`R_ALPHA_GPDISP - 0x4`.

**`Size = 12` / `Size = 8` are added here with no test that exercises them**
and are partially reverted by the very next commit (see below).

#### 68b1b88ea461 [Alpha] Add disassembler

**Regression: `Size = 12` is deleted from `JSR`.**

```
-let isCall = 1, Uses = [R27], Defs = [R26, R29], Size = 12 in
+let isCall = 1, Uses = [R27], Defs = [R26, R29], isCodeGenOnly = 1 in
```

`Size = 12` was added one commit earlier (`d00ec074d435`) because the code
emitter expands `JSR` into jsr + ldah + lda. Dropping it makes
`getInstSizeInBytes` report 4 for a 12-byte instruction, which feeds branch
range computation and `AlphaFrameLowering.cpp:99`. `git log -S` shows it is not
restored until commit #231 (`802ced3d6201`, "Declare the real size of the
multi-instruction call pseudos") — a 157-commit window in which the tree is
wrong. This looks like an accidental deletion while adding `isCodeGenOnly`;
fix it in this commit.

**Comment does not match the encoding it describes.**
`llvm/test/CodeGen/Alpha/branch-obj.ll`:

```
; the branch), so a backward branch to the loop header three instructions
; earlier encodes -6, not -5.
```

A displacement of -6 means the target is 6 instructions before the *next*
instruction, i.e. 5 instructions before the `bne` — not three. Either the
comment is wrong or the CHECK is; they cannot both be right.

**`decodeBranchTarget` produces a raw instruction-unit immediate**, so
`llvm-objdump` prints `bne $1, -6` rather than a target address. Every other
in-tree target either resolves the target with the symbolizer or prints the
computed address. Acceptable for a first cut, but say so.

**Disassembler test coverage is five instructions** and includes no invalid
encoding (no `Fail` path is exercised), no negative memory displacement, and no
FP register class decode other than `addt`.

#### c0680590ad5b [Alpha] Add assembly parser

**A generic MC change is bundled into a target commit, and it has no user in
this commit.** `MCAsmInfo::UseExclaimForSpecifier` and the
`getGNUBinOpPrecedence` change land here, but nothing sets
`UseExclaimForSpecifier = true` — `git log -S` shows the only assignment
arrives in `8b354e4310d1` ("Assemble the GNU as macros and procedure
directives"), far later. So this commit adds dead generic-MC API. Upstream will
want the `llvm/lib/MC` + `llvm/include/llvm/MC` hunks as a separate `[MC]`
commit, landed together with the target that sets the flag.

**`matchAndEmitInstruction` does not handle `Match_MissingFeature`.** The
switch handles `Match_Success`, `Match_MnemonicFail`, `Match_InvalidOperand`
and falls through to a generic "failed to match instruction". Assembling
`sextb $1, $2` without `+bwx` therefore reports "failed to match instruction"
instead of naming the missing feature. `ErrorInfo` is also computed and thrown
away — every other target uses it to point at the offending operand's `SMLoc`.

**Silent failure with no diagnostic.** In `parseOperand`:

```
  if (getParser().getTok().is(AsmToken::Dollar)) {
    ...
    return ParseStatus::Failure;
  }
```

Returning `Failure` without calling `Error()` produces an unattributed parse
failure for input like `addq $foo, $1, $2`.

**Churn against the two neighbouring commits.** This commit marks `LDQ_U`,
`STQ_U` and `JMP` `isCodeGenOnly = 1`; `b8c244111a8c` (two commits later)
un-marks all three. Those markings should never be made — reorder so
`parenGPR`/`memzGPR` come first, or squash `b8c244111a8c` into this one.

**Untested parser paths**: `MC/Alpha/basic.s` never exercises the `(base)`
zero-displacement form that `parseOperand` explicitly supports
(`if (getParser().getTok().is(AsmToken::LParen)) Off = 0`), nor a negative
displacement, nor an expression displacement.

Encodings in `basic.s` all verified: `and` 0x46110000, `ldq` 0xA4100008,
`stq` 0xB63E0010, `addt` 0x5A001400, `lda` 0x201F002A.

#### 986dd46fcfe5 [Alpha] Assemble relocation specifiers, ldgp and jsr

**Crash / UB on malformed assembly.** Both special cases index operands by
position and read them as the wrong kind without checking:

```
  if (Mnemonic == "ldgp" && Operands.size() == 3) {
    MCRegister Base = static_cast<AlphaOperand &>(*Operands[2]).getMemBase();
```

`getMemBase()` only asserts `Kind == Memory`. `ldgp $29, $30` (three operands,
last one a register) reads a union member that was never written — an assert in
a debug build, garbage in a release build. Same for the `jsr` case, which calls
`getReg()` on `Operands[1]` and `getMemBase()` on `Operands[2]`. Guard with
`isReg()` / `isMem()` and emit a real diagnostic.

**`ldgp`'s first operand is parsed and then ignored.** The expansion always
emits `ldah $29, ...` / `lda $29, ...` regardless of what the user wrote, so
`ldgp $1, 0($27)` silently assembles as `ldgp $29, 0($27)`. Either honour the
register or reject anything but `$29`.

**A relocation suffix on a non-relocatable operand is silently dropped.**

```
    static_cast<AlphaOperand &>(*Operands.back())
        .applySpecifier(Spec, getContext());
```

`applySpecifier` is a no-op for `Token` and `Register` operands, so
`addq $1, $2, $3 !literal` assembles cleanly with the suffix discarded.

**The `!seq` sequence number is consumed blindly:**

```
    if (getLexer().is(AsmToken::Exclaim)) {
      getParser().Lex(); // !
      getParser().Lex(); // number
    }
```

No check that the second token is an integer; `foo !literal!` swallows the
end-of-statement token.

**Test does not check what matters.** `MC/Alpha/reloc.s` checks four
relocation *names* and one symbol. It does not check the GPDISP addend, the
relocation offsets, or that the `!literal` on `ldq` landed on the right
instruction.

**`JSRasm` is a hack that is replaced two commits later** by `JSRr` with a
`parenGPR` operand (`b8c244111a8c`). Consider introducing `parenGPR` first so
this commit does not need the `Mnemonic == "jsr"` special case at all.

#### b8c244111a8c [Alpha] Disassemble jsr, jmp, ldq_u and stq_u

Encodings in the new disassembler test all verified: `jsr $26,($27)` =
0x6B5B4000, `jmp $31,($0),0` = 0x6BE00000, `ldq_u $1,0($16)` = 0x2C300000,
`stq_u $0,0($16)` = 0x3C100000.

**Commit message trails off into meta-commentary:**

> This gets them into the disassembler. It does not get them into the
> assembler matcher: parenGPR and memzGPR have no ParserMatchClass, so there is
> nothing for the matcher to match them with.

The first sentence restates the subject line. Keep the second (it is a real
limitation), drop the first.

**Structural**: as noted above, this commit reverts `isCodeGenOnly` markings
that `c0680590ad5b` added two commits earlier. Squash or reorder.

#### 1d852e99ad17 [Alpha] Support integer inline assembly operands

**The commit adds code the message says it does not add.** The message ends:

> Floating-point ("f") and memory ("m") operand constraints need further
> register-class and memory-operand handling and are not yet supported.

yet the diff adds `AlphaAsmPrinter::PrintAsmMemoryOperand`, which exists only
to print `"m"` operands. Without `SelectInlineAsmMemoryOperand` (added in the
next commit) it is unreachable dead code. Move it to `88616a50bb3d`.

**No test for the `MO_Immediate` path** in `PrintAsmOperand` (an `"i"`
constraint), which is one of the two cases the function handles.

#### 88616a50bb3d [Alpha] Support memory-operand inline assembly constraints

**Silently wrong output for a non-immediate displacement.**

```
  const MachineOperand &Disp = MI->getOperand(OpNo + 1);
  O << (Disp.isImm() ? Disp.getImm() : 0) << '(' ...
```

If the displacement is not an immediate (e.g. a global address that
`SelectADDRri` folded), this prints `0(...)` — a wrong address, no diagnostic.
Prefer an assert or `report_fatal_error`.

**The fallback in `SelectInlineAsmMemoryOperand` can produce a non-register
base:**

```
      if (!SelectADDRri(Op, Base, Offset)) {
        Base = Op;
```

`Op` here may be a frame index or a constant, and `PrintAsmMemoryOperand` then
calls `getRegisterName(MI->getOperand(OpNo).getReg())` on it.

**Test only covers displacement 0** (`ldq $0, 0($16)`), so the whole reason
this commit exists — a non-zero base+displacement — is untested.

#### d996db159df7 [Alpha] Support floating-point inline assembly operands

**Untested claim in a comment.**

```
      // An integer bound to an FP register (e.g. loading the FPCR) is 64-bit,
      // so treat i64 like f64.
      if (VT == MVT::f64 || VT == MVT::i64)
        return std::make_pair(0U, &Alpha::F8RCRegClass);
```

`F8RC` has value type list `[f64]` only, so binding an `i64` operand to it is
exactly the type mismatch this commit is fixing, in the other direction. There
is no test for `asm("..." : "=f"(i64var))`. Either add one or drop the `i64`
case.

**Two new 32-register classes duplicate `FPRC`.** `F4RC`/`F8RC` cover the same
physical registers as `FPRC` and exist only for inline asm; this enlarges the
generated register-class tables and the allocation-order/weight machinery for
the whole target. Worth a sentence in the message justifying it over the
alternative (returning a `VT`-appropriate subclass or overriding
`getRegClassFor`).

#### ddb35ca4231e [clang] Do not use musttail in the bytecode interpreter on Alpha

No objections. The message is slightly redundant ("the same way it does not on
PowerPC for the same reason" followed by "as the other targets in this list
do"); one of the two suffices.

#### 191e891a25c2 [clang][Alpha] Add target support

Data layout matches `TargetDataLayout.cpp:578` exactly — good, no
front-end/back-end mismatch.

**`__alpha_ev4__` is defined unconditionally.**

```
  Builder.defineMacro("__alpha_ev4__");
```

GCC derives `__alpha_ev4__` / `__alpha_ev5__` / `__alpha_ev6__` /
`__alpha_ev67__` (and `__alpha_bwx__`, `__alpha_cix__`, `__alpha_fix__`) from
the selected CPU. Hard-coding ev4 means `-mcpu=ev6` still advertises ev4, which
is exactly the sort of thing autoconf feature tests read. It is also not
mentioned in the commit message (which lists only "__alpha__/__alpha/_LP64")
and not covered by `init-alpha.c`.

**`isValidCPUName` accepts ev4..ev67 but nothing consumes the CPU.** There is
no `setCPU` / `fillValidCPUList` / feature derivation, so `-mcpu=ev6` is
accepted and ignored at the front end.

**`MaxAtomicPromoteWidth` / `MaxAtomicInlineWidth` are never set** (they
default to 0), so every `_Atomic` operation lowers to a libcall and
`__GCC_HAS_SYNC_COMPARE_AND_SWAP_*` are not defined — even though the back end
has ldl_l/stq_c and this chunk later teaches it sub-word atomics. Should be 64.

**`AlphaABIInfo` is an empty subclass of `DefaultABIInfo`** at this point.
Acceptable only because later commits fill it in; consider deferring the class
to `ef83df82488e`.

**Confusing pseudo-prefix in the test.** `clang/test/Preprocessor/init-alpha.c`
line "`// LP64: 64-bit long and pointer, 32-bit int.`" looks like a FileCheck
prefix directive but is a prose comment. Rename it (`// LP64 model: ...`) so it
cannot be mistaken for a check.

#### af0eb12ffa9a [clang][Alpha] Add driver support for alpha-linux-gnu

Values are right (`elf64alpha`, `/lib/ld-linux.so.2`, `lib` not `lib64`).

**Bulleted commit message.** LLVM commit messages are prose; the five-bullet
list of function names reads like a changelog. Fold to prose, and drop the
closing "Test in linux-ld.c checks the emulation and dynamic linker." — the
diff shows that.

**Test coverage**: `CHECK-ALPHA` checks `-m` and `-dynamic-linker` only. There
is no test for `getMultiarchTriple` or the `getOSLibDir` = `lib` decision,
both of which this commit adds and both of which are silent-misbehaviour
material.

#### ef83df82488e [clang][Alpha] Give Clang the {base, offset} va_list

**This commit ships the wrong ABI and `149ee63f25c7` fixes it 7 commits
later.** `__offset` is created as `Context->LongTy`; GCC's
`alpha_build_builtin_va_list` uses `integer_type_node`. `149ee63f25c7`'s own
message documents the consequence ("A gcc va_start writes four bytes at offset
8, and clang then read eight"). Squash `149ee63f25c7` into this commit — a
bisect point that defines a broken ABI type is worse than no commit at all,
and `__builtin_va_list` is not something to get wrong even transiently.

**`EmitVAArg` sign-extends where the back end zero-extends.** After
`149ee63f25c7`, clang does `Builder.CreateSExt(EffOffset, CGF.Int64Ty)` while
`LowerVAARG` uses `ISD::ZEXTLOAD` with a comment arguing zero-extension is
better. Both are correct for non-negative offsets, but pick one and say so in
one place.

**`test_struct` and `struct S3` are declared in this commit's test but only
used in `55f8ff30af5a`** — `struct S3 { long a, b, c; };` is added to
`alpha-varargs.c` here with no function using it.

#### 149ee63f25c7 [Alpha] Make va_list.__offset an int, as the ABI requires

The change itself is right, and `va-list-layout.c` is a good test (it pins
size, alignment, field offset and field width against measured GCC values).

**Should be squashed into `ef83df82488e`** (see above).

**`Fixes ALPHA-004.`** — an internal tracker ID with no meaning to anyone
reading `llvm-project` history. Remove it from this and every other commit in
the series.

**The message is six paragraphs for a five-line change.** The measurement line
and the int-vs-long rationale are worth keeping; the zero- vs sign-extension
paragraph belongs in the code comment (where it already is, verbatim), and the
qemu validation paragraph can be one sentence.

#### 55f8ff30af5a [clang][Alpha] Pass aggregates by value in the ABI

The coercion to `[N x i64]` matches how GCC lowers Alpha struct arguments, and
indirect aggregate returns are right for OSF.

**Scalar ABI is knowingly wrong at this point and fixed much later.** With
`DefaultABIInfo::classifyArgumentType`, `unsigned int` gets `zeroext`, but the
Alpha canonical register form of a 32-bit value is sign-extended regardless of
signedness (GCC's `alpha_promote_function_mode`). The tree's current
`Alpha.cpp` has `extendIntegerInRegister` doing exactly that, plus X_floating
handling — neither is in this commit. Since this commit's subject is "the ABI",
mention what is still wrong, or move the extension fix in front of it.

**No test for a struct larger than six quadwords**, i.e. the stack-spill case
the commit message explicitly claims to handle ("spilling to the stack past the
sixth"). `take24` is the largest tested.

**No test for a struct with a non-multiple-of-8 size** (e.g. `struct { char a;
short b; }`), where `(Size + 63) / 64` rounds up and the tail is padding.

#### a35542803717 [Alpha] Support local-exec thread-local storage

Relocation numbers verified against binutils (`R_ALPHA_TPRELHI` 39,
`R_ALPHA_TPRELLO` 40). PALcode format (opcode 0, 26-bit function) and
`call_pal 0x9e` (rduniq) are correct.

**Dead node type.** `AlphaISD::THREAD_POINTER` is added to the enum and to
`getTargetNodeName`, but nothing ever creates it — the thread pointer is
materialized directly as `getMachineNode(Alpha::RDUNIQ, ...)`. `grep -rn
THREAD_POINTER llvm/lib/Target/Alpha` at HEAD returns nothing, so it was
introduced dead here and deleted later. Don't add it.

**The `rduniq` is emitted before the model is checked**, so the
non-local-exec path builds a `RDUNIQ` machine node and then calls
`report_fatal_error`. Move the `TP` computation into the local-exec branch.

**`report_fatal_error("only local-exec TLS is supported on Alpha")`** — for a
user-triggerable condition (compiling `-fPIC` code with a `__thread`
variable), `reportFatalUsageError` / a proper diagnostic is the LLVM
convention, and no test exercises it.

#### b04619ec1634 [Alpha] Support initial-exec thread-local storage

`R_ALPHA_GOTTPREL` = 37 verified. STT_TLS typing is right and matches what
other ELF writers do.

**The initial-exec test is appended to `tls-local-exec.ll`.** A file named
`tls-local-exec.ll` should not contain `@ie = external
thread_local(initialexec)` and `read_ie`. Put it in `tls-initial-exec.ll`, or
rename the file to `tls.ll` when the first non-LE model arrives.

**Two consecutive switches on `Fixup.getKind()`** in `getRelocType` (one to set
STT_TLS, one to pick the relocation). Fold into one, or set the symbol type in
the existing switch arms.

**`const_cast<MCSymbol *>(...)` + `static_cast<MCSymbolELF *>`** — use
`cast<MCSymbolELF>` as RISCV/others do; the `static_cast` skips the type check.

#### 5db347c39bd4 [Alpha] Support general-dynamic thread-local storage

`R_ALPHA_TLSGD` = 29 verified.

**`TP` (and its `RDUNIQ`) is still computed unconditionally**, and the GD path
never uses it. It relies on DAG dead-node elimination to remove a machine node
that is marked `hasSideEffects = 1` and `Defs = [R0]`. If it ever survives, it
clobbers `$0` — the very register the `__tls_get_addr` return value comes back
in. Sink the `TP` computation into the two paths that use it.

**No `!lituse_tlsgd` on the jsr.** GAS emits `!tlsgd!N` on the `lda` paired
with `!lituse_tlsgd!N` on the `jsr`, which is what lets the linker relax
GD -> IE/LE. Without it the sequence is correct but unrelaxable. Worth an
explicit note in the message (the commit does note the analogous gap for
`lituse_jsr` in `88ea133580bc`, so the omission here reads as an oversight).

**Test checks only the instruction sequence**, not the relocations
(`-filetype=obj | llvm-readobj -r`), unlike `call-lituse.ll` later.

#### 845761ff9033 [Alpha] Support local-dynamic thread-local storage

`R_ALPHA_TLSLDM` 30, `R_ALPHA_DTPRELHI` 34, `R_ALPHA_DTPRELLO` 35 verified.
`adjustFixupValue` correctly puts `dtprelhi` in the `(Value + 0x8000) >> 16`
arm and `dtprello`/`tlsldm` in the `& 0xffff` arm.

**`report_fatal_error` becomes `assert(Model == TLSModel::LocalExec)`** in this
commit — good, and it retroactively confirms the two previous commits'
`report_fatal_error`s were placeholders. That is another argument for landing
the four TLS commits as one, or at least for using `assert` + a
`llvm_unreachable` style from the start rather than shipping a fatal error for
a legal input.

**~60 lines of `LDLd/LDQd/LDTd/LDSd/STLd/STQd/STTd/STSd` + eight `AddedComplexity`
patterns are a verbatim copy of the `t` (tprel) block** from `a35542803717`,
which is itself a copy of the `g` (gprel) block. Three near-identical copies of
the same eight defs and eight patterns is a multiclass waiting to happen.

#### d69fddb12236 [Alpha] Lower BR_CC to test-and-branch against zero

Branch opcodes verified: BLBC 38, BEQ 39, BLT 3A, BLE 3B, BLBS 3C, BNE 3D,
BGE 3E, BGT 3F.

**Comment claims nodes that do not exist.** Both in `AlphaISelLowering.h` and
`AlphaInstrInfo.td`:

```
  // Conditional branches that test a register against zero: equal, not-equal,
  // the signed relations, and the low-bit tests.
  BR_EQ, BR_NE, BR_LT, BR_LE, BR_GT, BR_GE,
```

There are no low-bit-test nodes; `blbc`/`blbs` are reached by a TableGen
pattern on `(AlphaBrNE (and GPRC:$Ra, 1))`. Drop ", and the low-bit tests"
from both comments, or say where the low-bit tests actually come from.

**Unsigned comparisons against 0/1 are not folded.** `x u< 1` (== `x == 0`) and
`x u> 0` (== `x != 0`) are not in the `V == 0/1/-1` tables. DAGCombine
canonicalizes most of these, but `SETULT`/`SETUGT` survive in some shapes and
then cost a `cmpult` + `bne`. Not a bug, but the commit message's framing
("only beq/bne were used, so a comparison against zero went through cmp + bne")
implies all comparisons against zero are now handled.

**No test for `le`** (`x <= 0` -> `bgt`) or for the `blbs` direction of the
low-bit test; the test covers `lt`, `ge`, `gt`, `eqz`, `blbc` and the
two-variable fallback.

#### 44625d2bc56a [Alpha] Add -mbuild-constants to materialize wide constants inline

**Regression: 32-bit constants with a 0x8000 high half stop being handled.**

```
-    if (!isInt<16>(Hi)) {
-      // Anything the ldah/lda pair cannot build goes in the constant pool ...
-      // also those whose high half is 0x8000, which the signed field of ldah
-      // cannot hold.
+    if (!isInt<32>(V)) {
```

The old condition sent `V` values whose `Hi` is `0x8000` (e.g. `0x7fffffff`,
`0x7ffffffb`) to the constant pool. The new condition only catches
`!isInt<32>(V)`, so those constants now fall past both `if`s to
`SelectCode(Node)` and hit "Cannot select". This is fixed at commit #129,
`9b9e5711781b` ("Materialize all 32-bit constants inline"), whose message says
verbatim: "does not fit ldah's signed 16-bit field, and previously fell through
instruction selection to a 'Cannot select' error". Fix it here — the
`buildConstant32` factoring that #129 performs is the right shape and belongs
in this commit.

**Signed overflow (UB) for `V` near `INT64_MAX`.**

```
  int32_t Lo32 = static_cast<int32_t>(V);
  int64_t Hi32 = (V - Lo32) >> 32;
```

For `V = INT64_MAX`, `Lo32 == -1` and `V - Lo32` overflows `int64_t`. It
happens to produce the right answer on wrapping hardware, but UBSan builds will
flag it and the compiler is entitled to assume it cannot happen. Do the
subtraction in `uint64_t`.

**Garbled comment in the test.** `build-constants.ll`:

```
  ret i64 81985529216486896 ; 0x0123456789abcdef0 truncated to 0x123456789abcdef0
```

`81985529216486896` is `0x0123456789ABCDF0`. Neither hex value in the comment
is that number (and `0x0123456789abcdef0` has 17 hex digits, so it is not a
64-bit value at all). Fix or delete.

**The two hard cases are tested only negatively.** `maxpos` and `minneg` — the
INT64_MAX/INT64_MIN cases the message calls out as the reason for the
double-`ldah` carry path — have only `INLINE-NOT: !gprel`. A build that emits a
wrong constant with no constant pool passes. Check the actual instruction
sequence (`ldah`/`ldah`/`lda`/`sll`/...).

#### 5fbe34714ba2 [clang][Alpha] Wire the -mbuild-constants driver flag

Clean; the driver test covers on/off/default. Two nits:

- The two options are inserted under the `// SPARC feature flags` comment in
  `Options.td`, inside `let Flags = [TargetSpecific]`. Give them their own
  `// Alpha feature flags` heading above the SPARC block, matching the pattern
  of every other arch there.
- `mno_build_constants` has no `HelpText`, so `clang --help` shows only half
  the pair. Most negative flags in that file follow the same (bad) pattern, so
  this is minor.

#### 88ea133580bc [Alpha] Tag direct calls with hint and lituse_jsr relocations

**Bug: the lituse relocation references a symbol literally named `.text`.**

```
    MCSymbol *TextSym = Ctx.getOrCreateSymbol(".text");
    const MCExpr *Use =
        MCBinaryExpr::createAdd(MCSymbolRefExpr::create(TextSym, Ctx),
                                MCConstantExpr::create(3, Ctx), Ctx);
```

`getOrCreateSymbol(".text")` returns a symbol *named* `.text`, which is not the
same thing as the section the instruction is in. With `-ffunction-sections`, or
in `.text.hot`, or in any non-`.text` section, this produces a relocation
against the wrong (or an entirely undefined) symbol. Use the current section's
begin symbol (`MCSection::getBeginSymbol()`), or emit the fixup with no symbol
and let the ELF writer pick the section symbol.

**`R_ALPHA_LITERAL` is dropped from `needsRelocateWithSymbol` for all uses, not
just calls.** The comment justifies it in terms of call relaxation, but the
change also affects every GOT load of a global's address. It is probably
correct (a section+addend GOT entry resolves to the same address for a
non-preemptible symbol), but no test covers the data case, and the commit
message does not mention that the change is not call-specific.

**`JSRd`/`JSRdl` are three-instruction pseudos with no `Size`**, the same
problem as `JSR` above; both are given `Size = 12` only at commit #231.

**The test checks names, not values.** `call-lituse.ll` never checks that the
LITUSE addend is 3 — the use type that makes it a *jsr* lituse rather than a
base/bytoff/jsrdirect one — nor which symbol the LITUSE is against, which is
the buggy part.

#### 7982d786041e [Alpha] Promote i1 loads to byte loads

Correct and minimal. Two nits:

- The test runs only with `-mcpu=ev6` (BWX). The pre-BWX path
  (`ldq_u`/`extbl`) is the more fragile one and is not covered.
- `use_bool`'s only check is `CHECK: ldbu`, which the first function already
  established; the function adds no coverage as written.

#### 7f87b1815dfb [Alpha] Implement branch analysis

**`analyzeBranch` is wrong as committed and is fixed by the very next commit.**
It looks back exactly one instruction, so a block ending `beq / bne / br` is
reported as a two-way block carrying only the second condition, losing the
first branch's edge. `a715d62c5180` fixes precisely this and says "Every other
in-tree target ends the two-terminator case with this check." Squash
`a715d62c5180` into this commit.

**`AllowModify` is accepted and ignored** (also fixed by the next commit).

**`removeBranch`'s loop is contorted for no reason:**

```
    MachineBasicBlock::iterator ToErase = I;
    bool AtBegin = I == MBB.begin();
    if (!AtBegin)
      --I;
    ToErase->eraseFromParent();
    ...
    if (AtBegin)
      break;
    I = MBB.getLastNonDebugInstr();
```

The `--I` is dead because `I` is reassigned at the bottom, and the
`AtBegin` early break is redundant with the loop condition. The next commit
deletes all of it — again, squash.

**`insertBranch` has none of the asserts every other target has** (`TBB`
non-null, `Cond.size()` of 0 or 2); added by the next commit.

#### a715d62c5180 [Alpha] Refuse to analyze a block with three terminators

The fix and the `.mir` test are both good — a `.mir` test is exactly right for
a shape that ISel cannot produce.

**This is a fixup commit for `7f87b1815dfb` and should be squashed into it.**
Dated a month later, it repairs four separate defects introduced there
(three-terminator handling, ignored `AllowModify`, the dead `AtBegin/--I`
dance, missing asserts) plus the test text.

**`Fixes ALPHA-013.`** — internal tracker ID, remove.

**The message is a five-topic changelog.** Paragraphs 2-4 each describe an
unrelated cleanup ("Also in the same function: ...", "removeBranch had an
AtBegin/--I dance ...", "insertBranch was missing the two asserts ..."), and
paragraph 5 is commentary on the test file's prose. If this must stay a
separate commit, split it: the three-terminator fix is the only one that is a
correctness bug.

#### 67b1a5df7e81 [Alpha] Add mov and clr assembler pseudo-instruction aliases

Encodings verified: `bis $31,$1,$2` = 0x47E10402, `bis $31,$31,$3` =
0x47FF0403.

**The comment says the same thing three times:**

```
// mov copies a register (bis $31, src, dst); clr zeros one; nop is bis of $31.
// bis operands are (Rc, Ra, Rb); the result register is Rc.  mov copies Rb
// into Rc (bis $31, Rb, Rc); clr zeros Rc.  (nop is the def above.)
```

Sentence 1 and sentence 3 are the same statement; the `nop` remarks are
irrelevant to these two aliases. One line suffices: "bis operands are
(Rc, Ra, Rb); mov is bis $31, src, dst and clr is bis $31, $31, dst."

**`mov` with an immediate is not handled.** GAS accepts `mov 5, $1`
(-> `bis $31, 5, $1` via the literal form). Since the motivating case is
"ordinary inline asm", the immediate form is likely to show up too. A second
`InstAlias` over `BIS`'s literal form would cover it.

#### 873e86711b9b [Alpha] Select sub-word atomic loads

Correct — ordering is handled by `shouldInsertFencesForAtomic` returning true
(`AlphaISelLowering.h:206`, added earlier in the series), so a bare `ldbu` here
is right for the relaxed case and the fences are inserted by AtomicExpand.

**Only the zero/any-extending fragments are covered.** There are no
`atomic_load_asext_8/16` patterns. Reachable only if a sign-extending atomic
sub-word load survives to ISel; worth a comment saying why it cannot.

**Test asymmetry**: `l8` checks `ldq_u` *and* `extbl` in the NOBWX run, `l16`
checks only `extwl`. Add the `ldq_u` check for symmetry.

#### f406ccccb47c [Alpha] Select sub-word atomic stores

The change from `Predicates = [SafeBWStore]` on the pseudo to `Predicates` on
the patterns is the right way to make the custom inserter available to both
users. Reasoning about the pre-BWX case (plain RMW is not atomic, so the
ldq_l/stq_c loop is required) is correct.

**No test for `-mattr=+safe-bwa`** in this file, so the commit's claim that the
pseudo is now shared between `-msafe-bwa` and pre-BWX atomics is only half
tested; a regression that broke the `SafeBWStore` patterns would be caught only
by whatever test `a5407c...`/the `-msafe-bwa` commit left behind.

**`BWX: stb $17, 0($16)` hard-codes the register allocation.** Elsewhere in
this series the tests use `{{\$[0-9]+}}` for allocated registers; here `$17`
and `$16` are argument registers so it is stable, but the pattern is
inconsistent with `atomic-load-subword.ll` right next to it, which checks a
bare `ldbu` with no operands at all.

---

#### Cross-cutting recommendations

1. **Squash the three fixup commits backwards**: `149ee63f25c7` into
   `ef83df82488e`, `a715d62c5180` into `7f87b1815dfb`, and the `Size = 12`
   restore from #231 into `68b1b88ea461`. Fix the constant-pool regression in
   `44625d2bc56a` rather than at #129.
2. **Remove `Fixes ALPHA-NNN.`** from every commit message in the series.
3. **Split the generic MC changes out of target commits**: `MCAsmInfo.h` +
   `AsmParser.cpp` from `c0680590ad5b`, and `TargetInfo.h` +
   `ASTContext.cpp` (the `AlphaABIBuiltinVaList` kind) from `ef83df82488e`
   should each be a separate `[MC]` / `[clang]` prefixed commit, since
   reviewers for those areas differ.
4. **Relocation tests should check values, not just names.** Three commits
   (`d00ec074d435`, `986dd46fcfe5`, `88ea133580bc`) test relocations with bare
   `CHECK: R_ALPHA_*` lines while the interesting part is the addend (GPDISP 4,
   LITUSE 3) or the symbol (`.text` vs the real section).
5. **Trim the commit messages.** Several run 4-6 paragraphs where two would do,
   and most end with a "Validated under qemu-alpha: ..." paragraph that repeats
   itself commit to commit. One sentence ("Validated under qemu-alpha.") is
   enough after the first few.

### Call-frame information, intrinsics and builtins, misaligned access, scheduling (commits 101–130)

#### cd617a876f3a — [Alpha] Support a frame pointer and dynamic stack allocation

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

#### f7ef85d7b231 — [Alpha] Emit DWARF call-frame information

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

#### 3037704af620 — [Alpha] Emit epilogue call-frame information

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

#### 9c4ba65e4603 — [Alpha] Support stack frames larger than 32KiB

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

#### 0a0690123ab2 — [Alpha] Assemble hand-written context-save assembly

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

#### 2f67d49efc2e — [Alpha] Add integer and floating-point negate/copy pseudo aliases

Semantics verified: `fabs` → `cpys $f31, Rb` (sign from `$f31` = positive,
magnitude from Rb) ✓; `fmov`/`fneg` → `cpys`/`cpysn Rb, Rb` ✓;
`sextl` → `addl $31, Rb` ✓; `negs`/`negt` → `subs/subt $f31, Rb` matches GAS.

**Test.** The commit message claims "Each is byte-identical to the GNU
assembler's output", but the ten new CHECK lines in `pseudo-asm.s` check only
the *printed* base instruction — none has `# encoding:`, unlike the six lines
directly above them in the same file. Nothing in the test verifies the bytes.
Add `# encoding:` to at least the non-obvious ones (`negq`, `sextl`, `fabs`).

---

#### 1af22d79e410 — [Alpha] Add the llvm.alpha.* intrinsics

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

#### e3cca3af6f8b — [clang][Alpha] Implement the __builtin_alpha_* functions

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

#### 104c1f65267d — [Alpha] Lower misaligned loads and stores with ldq_u and extract/insert

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

#### 0e0d39c89651 — [Alpha] Add scheduling models for the 21064, 21164 and 21264/21364

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

#### 01b5aa0ae6f1 — [Alpha] Refine the EV6 mispredict and cross-cluster modeling

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

#### a7dae61f34ba — [Alpha] Schedule after register allocation for the in-order cores

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

#### 3a1010615a8a — [Alpha] Emit lituse_tlsgd/lituse_tlsldm for dynamic TLS calls

Correct; use types 4/5 match the GNU toolchain, and the tests genuinely check
the relocation with `llvm-readobj -r` (the strongest tests in this chunk).

- `JSRtlsgd` and `JSRtlsldm` are byte-identical definitions differing only in
  the emitter's addend selection. Consider one instruction with a use-type
  operand, or at minimum note why two are needed.
- No test asserts `JSRd`/`JSRdl` still emit addend 3 after the emitter
  refactor. If an existing test covers it, fine; otherwise add a `RELOC` line.

---

#### bb58f0db8237 — [Alpha] Emit IEEE software-completion trap qualifiers for -mieee

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

#### 1717696878a2 — [clang][Alpha] Add the -mieee and -mieee-with-inexact flags

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

#### 37b6ead2f875 — [Alpha] Lower llvm.prefetch to R31/F31 loads

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

#### 06708fb205ca — [Alpha] Use itoft/ftoit for integer/FP moves with the FIX extension

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

#### b6fbd19b4eeb — [Alpha] Emit a .arch directive for the enabled extensions

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

#### 45dfd5063fae — [Alpha] Feed floating-point compares straight into fcmovne

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

#### ee52db3535d2 — [Alpha] Support 4-byte atomics with ldl_l/stl_c

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

#### 95ca3bea6cc1 — [Alpha] Materialize the float constants that need no load

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

#### d2579856cf99 — [Alpha] Use itofs/ftois for f32/i32 bit casts with the FIX extension

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

#### df6f7d190ce3 — [Alpha] Lower any byte-granular AND mask to a single zapnot

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

#### 327dd5f881db — [Alpha] Add the longword scaled add/subtract instructions

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

#### 167a60b4f245 — [Alpha] Extract a byte/word/longword at a constant offset with ext

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

#### 9d80d1057b47 — [Alpha] Select ext/ins/msk for byte ops at a variable position

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

#### c4997c8611ed — [Alpha] Fold a sign-extended 32-bit operation into addl/subl/mull

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

#### 4b5f9b8bc17f — [Alpha] Strength-reduce multiply by a constant

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

#### 9b9e5711781b — [Alpha] Materialize all 32-bit constants inline

The best commit in the chunk: real bug ("Cannot select" for high half 0x8000),
clean refactor (hoisting the `add32` lambda into `buildConstant32`), stated
side effect, and five new tests that pin exact immediates including both
boundary cases.

One nit: the comment on the `int32_min` test says "INT32_MIN = 0x80000000
sign-extends from a single ldah", but the test body is `ret i64 -2147483648`,
i.e. `0xFFFFFFFF80000000`. Reword to avoid conflating the 32-bit pattern with
the 64-bit value.

---

#### 44a29238297d — [Alpha] Store zero from the zero register

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

#### Cross-cutting recommendations

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

### Cost model, tail calls, outliner, branch relaxation, assembler directives (commits 131–160)

#### 91d8b67e3eda — [Alpha] Add a target transform info cost model

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

#### 8d8e56a1c8fc — [Alpha] Test inline memcpy/memset expansion and min/max lowering

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

#### 3fec1c41ad39 — [Alpha] Support sibling and tail calls

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

#### d4fce8bec379 — [Alpha] Signed division by a constant via magic multiply

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

#### 3cb33f89d6f7 — [Alpha] Factor a constant multiply into scaled-add chains

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

#### 3e1b8960e0e7 — [Alpha] Rematerialize constants instead of spilling them

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

#### acfc9cad60bb — [Alpha] Model truncation as free and sign-extension as cheaper

**Must be squashed with 6694f35fe1e1 (below).**  As it stands this commit
introduces a wrong `isSExtCheaperThanZExt` and a test that, by the next
commit's own admission, cannot detect the bug.

* `AlphaISelLowering.h:240` — `isSExtCheaperThanZExt` returning true for every
  integer pair is wrong for i1/i8/i16 (fixed 8 commits later).
* `ext-free.ll` (as added here) does not reach the hook at all.

#### 6694f35fe1e1 — [Alpha] Prefer a sign extension only from i32

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

#### 3f92812b20ab — [Alpha] Reassociate operation chains with the machine combiner

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

#### 542077e8083a — [Alpha] Support the machine outliner

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

#### e3a4cf0a6d35 — [Alpha] Relax branches to out-of-range targets

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

#### 0ad758bec362 — [Alpha] Fold a small constant into a comparison

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

#### c864fb8e4ad9 — [clang][Alpha] Accept the GCC inline-asm constraint letters

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

#### 156e4bdfe5e8 — [clang][Alpha] Derive the extension set and macros from -mcpu

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

#### 522419f3c6b0 — [Alpha] Reserve registers for -mno-fp-regs and -ffixed-$<n>

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

#### 14fa704d3919 — [clang][Alpha] Add -mno-fp-regs and -ffixed-$<n>

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

#### a0713494cac5 — [Alpha] Accept the GCC inline-asm constraint letters

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

#### 5f3b5032c9ed — [Alpha] Name a physical register in inline asm and a register variable

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

#### 4d135b7ab1d1 — [Alpha] Lower a block address

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

#### d3c74d373b76 — [Alpha] Lower the return and frame address builtins

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

#### 816220a5e5c3 — [Alpha] Expand a single-bit sign extend

* **Code comment contradicts the commit message and the test.**
  `AlphaISelLowering.cpp:129`: `// No single-bit sign-extend instruction; expand
  it to an sll/sra pair.` — but the message says (and `sext-inreg-i1.ll` checks)
  `and`/`subq`.  Fix the comment.
* Otherwise minimal and correct.  Consider adding an `-mcpu=ev4` RUN line for
  symmetry with the neighbouring i8/i16 `SIGN_EXTEND_INREG` handling, which is
  BWX-conditional two lines above.

#### 55592232601b — [Alpha] Assemble PALcode calls

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

#### bd89c4b75bce — [Alpha] Assemble the kernel's hand-written assembly

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

#### 42b87b72190d — [Alpha] Make .align count a power of two, as GNU as does

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

#### ab9cbc94f3c2 — [Alpha] Pad code alignment with unop

* Correct: `unop` = `ldq_u $31, 0($30)` = 0x2ffe0000, matching GNU as.
* `writeNopData` still does `OS.write_zeros(Count % 4)` for a sub-word tail;
  pre-existing, but now that the filler is being made GNU-as-compatible it is
  worth a word on why the remainder is zeros rather than `.byte 0x00` padding
  the way gas does.
* The test is good (it distinguishes `nop` from the filler).  Consider also
  checking `-filetype=obj` alignment padding in a `.text` subsection.

#### cb813e89fd28 — [Alpha] Accept a $-prefixed local label in an operand

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

#### 8b354e4310d1 — [Alpha] Assemble the GNU as macros and procedure directives

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

#### a5df8ad81625 — [clang][Alpha] Forward -mtune to -tune-cpu

* Correct and minimal.
* `alpha-mtune.c:1-2` uses the deprecated `-target alpha-linux-gnu` spelling;
  new tests should use `--target=`.  (The neighbouring `alpha-fixed-regs.c` in
  14fa704d already uses `--target=`, so the series is inconsistent.)
* `-mtune=<garbage>` is forwarded unvalidated; the diagnostic then comes from
  LLVM as an obscure "not a recognized processor" warning.  Other targets
  validate against `isValidCPUName` — cheap to add here since it exists.
* No test that `-mtune` alone leaves `-target-cpu` at the default; the SPLIT
  case only covers `-mcpu` + `-mtune`.

#### e3edbce2ea45 — [Alpha] Record each function's procedure kind in st_other

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

#### 0ff321c4e5c5 — [Alpha] Keep the entry ldgp as the first instruction

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

### GP addressing, trap modes, f128 through OTS, long double (commits 161–190)

#### `c09d8db166ac` — Address a global gp-relative when the linker resolves it

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

#### `60f44ff8bf5e` — Do not address a preemptible global GP-relative under -msmall-data

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

#### `6ea39c7f99c0` — [clang] Add -msmall-data/-mlarge-data

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

#### `daeff8b04ab4` — Add -msmall-text single-instruction calls

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

#### `058c729e3e1a` — [clang] Add -msmall-text/-mlarge-text

`alpha-text-model.c` uses the deprecated `-target alpha-linux-gnu` spelling
(also in `f76f61f6f042`, `ff9e7a19f6c7`); other tests in this chunk use
`--target=`. Normalise on `--target=`.

Adjacency to `daeff8b04ab4` is right — keep the split.

---

#### `129d23d65d43` — Assemble the ldi/ldiq load-immediate pseudo

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

#### `96c00ba31848` — Add the floating-point trap and rounding qualifiers

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

#### `bb3c78006079` — Insert trap barriers for precise arithmetic traps

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

#### `f76f61f6f042` — [clang] Add -mfp-trap-mode, -mfp-rounding-mode, -mtrap-precision

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

#### `a72d3cda7a3b` — Give -mieee-conformant the meaning gcc documents

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

#### `515391beac5f` / `f5a303bc0d8f` — precise-trap processors, then un-marking generic

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

#### `807c84d1e95a` — Add -msafe-partial atomic misaligned stores

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

#### `beb2e35cdf43` — [clang] Add -msafe-partial

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

#### `0443b4690da9` — [clang] Define __LONG_DOUBLE_128__ and accept -mlong-double-128

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

#### `ec861e1e8e2e` — [clang] Pass long double by reference

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

#### `d1fe4c9bdf44` — Fetch a _Complex va_arg from the floating-point save area

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

#### `1d9f5946aaa9` — Assemble mov with an immediate source and $rN register names

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

#### `4aaeec2b8dda` — Support the v/a/b/c single-register inline-asm constraints

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

#### `f450dca2aec8` — Lower the thread-pointer intrinsics and builtin

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

#### `9ac03124f47e` — [clang] Extend sub-64-bit arguments and returns

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

#### `da307a314e61` — Lower f128 arithmetic to the Alpha OTS runtime

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

#### `0d075126ffd7` — Convert between f128 and the other types through OTS

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

#### `f94dcafc5813` — Compute the OTS X_floating mode argument the way gcc does

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

#### `e9c48ebf597a` — Compare f128 values through OTS

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

#### `04341794f7a7` — Do f128 sign operations without a call

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

#### `af4734b01f9e` — [clang] Advertise lock-free atomics

No correctness issues. `MaxAtomicInlineWidth = 64` with sub-word CAS expanded to
a masked longword loop is consistent with the rest of the backend, and defining
the four `__GCC_HAVE_SYNC_COMPARE_AND_SWAP_*` macros in `getTargetDefines` is the
established per-target idiom (SystemZ, LoongArch, M68k all do it).

Minor: the code comment duplicates the commit message and the test's header
comment word for word ("ldl_l/stl_c and ldq_l/stq_c give native 4- and 8-byte
compare-and-swap, and the sub-word cases are expanded to a masked longword loop,
so every size … is lock-free and inlined"). Keep it in one place.

---

#### `46782716b106` — [clang] Advertise strict floating point

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

#### `cf27fe3719cc` — Custom-lower STRICT_FSETCC/STRICT_FSETCCS for f32 and f64

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

#### `ff9e7a19f6c7` — [clang] Map -Wa,-mevN assembler ISA flags to target features

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

### Exception handling, fixups, assembler directives, st_other, i128 (commits 191–220)

#### `acf0a0870278` — [Alpha] Take the ISA names and their meanings from GNU as

**BUG (correctness, Alpha-only regression).**
`clang/lib/Driver/ToolChains/Clang.cpp:2669`:

```cpp
if (StringRef Cpu; Value.consume_front("-m") && (Cpu = Value, true)) {
  ...
  } else {
    break; // Not an ISA name; fall through to the generic handling.
  }
```

`Value` is the loop variable of `for (StringRef Value : A->getValues())` (line 2643).
`consume_front` **mutates it in place**. On the `break` path `Value` no longer starts
with `-m`, so the shared handling below the switch — which tests
`Value.starts_with("-mcpu")`, `Value.starts_with("-march")`,
`Value.starts_with("-msoft-float")`, etc. — never matches on Alpha. `-Wa,-mcpu=ev6`
falls all the way to the final `else` and produces
`error: unsupported argument 'cpu=ev6'` with the `-m` chopped off the message.
Fix: don't consume from the loop variable —

```cpp
StringRef Cpu = Value;
if (Cpu.consume_front("-m")) { ... }
```

**Style.** The `if (StringRef Cpu; cond && (Cpu = Value, true))` idiom — an if-init
plus a comma operator used purely to smuggle an assignment into the condition — will
draw a review comment on its own. There is no reason not to write a plain nested `if`.

**Tests.** `clang/test/Driver/alpha-wa-isa.c` — the `BASE`/`BWX`/`MVI` prefixes consist
only of `-NOT` lines (`BASE-NOT: "+bwx"`, `BASE-NOT: "+mvi"`). A `-NOT`-only FileCheck
invocation passes trivially if the driver produced *nothing at all*. Add one positive
anchor per prefix (`BASE: "-cc1as"` or `BASE: "-triple" "alpha`), otherwise these three
prefixes cannot fail.

**Verify.** `-mall`/`.arch all` is mapped to base ISA only. GNU as's `cpu_types[]` entry
for `"all"` should be confirmed; if it is `AXP_OPCODE_ALL` rather than
`AXP_OPCODE_BASE` this is backwards, and both the driver table and the `.arch` table
would be wrong. The commit message asserts `.arch` "gains … all, which it did accept"
but never states what it grants.

**Commit message.** Otherwise good — explains the *why* (pca56 has MVI), the two
non-1:1 mappings, and how it was validated. Drop `Fixes ALPHA-027.`

---

#### `1e51a0676963` — [Alpha] Deliver the exception pointer and selector in $a0 and $a1

Correct and minimal. Two nits:

* The four-line comment in `AlphaISelLowering.h:166-169` and the two inline comments in
  `AlphaISelLowering.cpp:417,421` (`// $a0 -- EH_RETURN_DATA_REGNO(0) = 16`) say the same
  thing three times, and the `.ll` test repeats it a fourth. Keep one.
* `llvm/test/CodeGen/Alpha/eh-regs.ll` has `CHECK-LABEL: caught:` and **no other CHECK**.
  It asserts nothing about $16/$17 — it only proves the function compiles. If
  `getExceptionSelectorRegister` were reverted the test would still pass. Add a check
  that the selector arrives in `$17` (e.g. `CHECK: addl $17` / `CHECK: sextl $17`).
  As written the commit's functional change is untested.

---

#### `7a39491dc76a` — [Alpha] Reload the global pointer at a landing pad

Real bug, well diagnosed, and the encoding is right: `INSN_BR_GP = 0xc3a00000` is
`br $29, .+4` (opcode 0x30<<26 | Ra=29<<21 | disp=0), and `Size = 12` matches
br+ldah+lda.

**Layering.** `emitEHPadGPReloads()` walks *every* block of the function but is called
from `emitPrologue()` (`AlphaFrameLowering.cpp:143`), inside `if (AFI->usesGP())`.
A whole-function transform driven off the entry-block prologue hook is surprising;
`processFunctionBeforeFrameFinalized` or a small pass is the conventional place.
Also, if a function has an EH pad but `usesGP()` is false, no reload is emitted — that
is probably safe but it is an undocumented invariant, not an argued one.

**Comment.** `AlphaMCCodeEmitter.cpp:27` — "A zero displacement makes it fall through to
the ldah it puts the address of into $29" is garbled; rewrite.

**Duplication.** The same nine-line rationale (libgcc `$26` vs libunwind `$28`,
`alpha_pad_function_end`) appears verbatim in the commit message, in
`AlphaFrameLowering.cpp:107-114`, in the `LDGPself` td comment
(`AlphaInstrInfo.td:738-752`), and again in `eh-pad-ldgp.ll:7-12`. Pick one home.

**Test.** Good — the OBJ prefix catching the missing MC emitter arm is the right
instinct. `; OBJ: 00 00 a0 c3` is a bare byte match with no address anchor; consider
`OBJ: <caught>:` before it.

---

#### `0a630862b917` — [Alpha] Convert between f32 and f128

**SPLIT.** This is two independent changes with two independent tests:
(a) f16/f32 ↔ f128 conversion support, (b) replacing `getSimpleValueType()` with
`getValueType()` in five f128 hooks to stop an assert/crash on extended types
(`icmp ne i65`). (b) is a crash fix that stands on its own and is what the
`icmp_i65` test covers; it should land first, separately.

**Gap.** `FP_ROUND` accepts only `DstVT == f64 || DstVT == f32`. `fptrunc fp128 to half`
therefore escapes to the generic path and calls `__trunctfhf2`, which — by the very
argument of `3647f8a10e60` — does not exist in Alpha's libgcc. The `FP_EXTEND` side
handles f16; the `FP_ROUND` side does not. Untested either way.

**Strictness.** The comment claims f32→f64 is exact "so use non-strict extends even
inside a constrained operation". True for the value, but the non-strict `FP_EXTEND`
is now unchained inside a `STRICT_FP_EXTEND` expansion and is free to be hoisted
across an FP-environment write. Worth a sentence saying that is intentional and
harmless, or use the strict node.

**Test.** `icmp_i65` is `CHECK-LABEL` only — a crash test. That is legitimate but say so
(`; This must not crash.`) rather than leaving a label with no assertions.

---

#### `dd2e8a4e012e` — [Alpha] Answer the remaining f128 condition codes

**BUG (or the commit message is wrong — one of the two).** The new comment at
`AlphaISelLowering.cpp:1360-1363` states:

> `_OtsEqlX` returns -1 (sign bit set) when either operand is NaN, and 0 or 1
> (sign bit clear) for ordered operands.

The pre-existing comment 30 lines above (`AlphaISelLowering.cpp:1335-1337`) states the
opposite:

> if either operand is NaN, `_OtsEqlX` returns 0, so negating gives 1, which is
> correct for an unordered compare.

If the new claim is the true one, then every `Negate`-based unordered code is broken:
for `fcmp une` with a NaN operand, `_OtsEqlX` returns `-1`, `XOR 1` gives `-2`, and
bit 0 is **0** — the branch/select sees *false* where the IR requires *true*. The same
applies to `SETUGT`/`SETUGE`/`SETULT`/`SETULE`/`SETUEQ` via `_OtsLeqX`/`_OtsLssX`/
`_OtsGeqX`/`_OtsGtrX`/`_OtsNeqX`. This commit is where the `-1` semantics is first
asserted, so it should either fix those six cases (mask with `& 1` after the XOR, or
extract the sign bit first) or correct the claim.

No test in `f128-compare.ll` feeds a NaN to any predicate — every test uses opaque
`load fp128` operands and only checks which `_Ots*` symbol is called. The two new tests
(`uo_f128`, `o_f128`) check `srl {{.*}}, 63,` but not the `xor` that distinguishes
`SETO` from `SETUO`, so `o_f128` and `uo_f128` have *identical* CHECK lines and neither
would fail if `Negate` were dropped from `SETO`.

The six NaN-free codes (`SETEQ`…`SETGE`) are added with no test at all; the commit
message says `llvm.fabs.f128` of a constant reaches them, which would make a good test.

---

#### `3647f8a10e60` — [Alpha] Keep an f128 conversion out of the memory access next to it

Fix is right and the reasoning is clear.

**BUG (test).** `llvm/test/CodeGen/Alpha/f128-mem-convert.ll:9-13`:

```
; CHECK-NOT: __extendhftf2
; CHECK-NOT: __extendsftf2
...
; CHECK-LABEL: ext_from_load_f32:
```

A `CHECK-NOT` that precedes the first positive directive is only checked against the
text *before* the first match — i.e. the `.text`/`.globl` preamble. All five are
effectively no-ops; the test cannot detect the regression it exists for. Use
`; RUN: … | FileCheck %s --implicit-check-not=__extendsftf2 --implicit-check-not=…`
or place each `CHECK-NOT` inside a function's region.

**Scope.** `setTruncStoreAction(MVT::f64, MVT::f32, Expand)` is not f128-specific — it
affects every `store float (fptrunc double %x)` in the target. It is the right setting
(Alpha has no truncating FP store), but it belongs in the commit that establishes the
target's basic FP legality, not in an f128 commit.

---

#### `5bb3fc961dbd` — [Alpha] Add R_ALPHA_GPREL16 and R_ALPHA_GPREL32 fixup support

**SPLIT (three changes, one of them undocumented).** The commit does:
1. add `fixup_alpha_gprel16` / `fixup_alpha_gprel32`;
2. **remove** `R_ALPHA_GPRELHIGH`/`GPRELLOW` from `needsRelocateWithSymbol()` — a
   behaviour change with real consequences (the `.L0` symbolizer problem), which is
   what the only test in this commit covers;
3. **add** `R_ALPHA_BRSGP` to `needsRelocateWithSymbol()` — mentioned nowhere in the
   commit message, and its test (`llvm/test/MC/Alpha/brsgp.s`) lands three commits
   later in `75e8a679a184`.

(3) at minimum must be called out; ideally (2) and (3) are one commit and (1) another.

**Untested feature.** Neither new fixup kind is exercised here. `!gprel` is not
accepted by the parser until `75e8a679a184` and `.gprel32` does not exist until then
either, so as landed this commit adds two dead enumerators plus dead
`AlphaELFObjectWriter` arms. Either fold it into `75e8a679a184` or reorder.

**Likely bug.** `fixup_alpha_gprel32` is deliberately left out of the `AlwaysReloc` list
in `AlphaAsmBackend.cpp:116`, justified as:

> gprel32 does not need to: it is a data fixup, and the value it names is in another
> section from the one holding it, so it always relocates anyway.

That reasoning does not hold in general. A GP-relative value is `sym + addend - GP`;
`GP` is never known at assembly time, so resolving a `.gprel32` locally is *always*
wrong, not merely usually. `.gprel32 1f` with `1:` in the same section (perfectly legal
assembly) will be folded to a section-relative offset with no relocation. Put
`fixup_alpha_gprel32` in `AlwaysReloc` and delete the paragraph.

**Nit.** `Infos[]` in `getFixupKindInfo` is indexed positionally by
`Kind - FirstTargetFixupKind`, so its order must track the `enum Fixups` order in
`AlphaFixupKinds.h` exactly. Both insertions here happen to be consistent, but this is
a silent-corruption trap; a `static_assert` on `std::size(Infos) == NumTargetFixupKinds`
would at least catch a missing row.

---

#### `5ce9c6144c32` — [Alpha] Encode an immediate that is not a relocation but not a constant either

The strongest commit in the chunk: real bug, concrete user impact (libffi →
every `ctypes` call), minimal fix, and a test with both a positive and a negative case.

**Gap.** Neither field is range-checked. `adjustFixupValue` does
`return Value & 0xffff;` for `disp16` and `return (Value & 0xff) << 13;` for `lit8`
(`AlphaAsmBackend.cpp:45-49`). A label difference larger than the field silently
truncates — the same class of bug the commit is fixing, one step later. GNU as
diagnoses `operand out of range`. Add `isInt<16>` / `isUInt<8>` checks and a test.

**Nit.** `{"fixup_alpha_lit8", 13, 8, 0}` declares `TargetOffset = 13` while
`adjustFixupValue` already returns the value pre-shifted by 13; `applyFixup` ignores
`TargetOffset` (it ORs all four bytes), so the two never collide today, but the
duplication is a landmine for anyone who later switches to the generic apply path.

---

#### `bd5c29e1b543` — [Alpha] Assemble floating-point qualifier suffixes

**SPLIT.** Title and message are about qualifier suffixes, but the diff also adds ten
instructions that have nothing to do with them:
`S4ADDQi`, `S8ADDQi`, `S4SUBQi`, `S8SUBQi`, `S4ADDLi`, `S8ADDLi`, `S4SUBLi`, `S8SUBLi`
(`AlphaInstrInfo.td:403-418`) and `CMOVLBS`/`CMOVLBC` register forms
(`AlphaInstrInfo.td:1896-1900`). None is mentioned in the commit message and **none is
tested** — `fp-qual-explicit.s` covers only `cvttq/svid`, `cvtqt/d`, `cvtst/s`.
Move them to their own commit with a test.

**Test weakness.** `fp-qual-explicit.s` checks raw bytes only:

```
# CHECK: e0 fd e1 5b
```

It never checks the disassembled mnemonic, so it cannot detect that the disassembler
prints garbage for these forms. Since the file runs `llvm-objdump -d`, checking
`e0 fd e1 5b {{.*}}cvttq/svid` costs nothing and covers the round trip. The three
CHECKs are also not `CHECK-NEXT`, so they would pass in any order.

**Nit.** `DIVTC` is defined with a hardcoded `"divt/c"` mnemonic here; one commit later
`9ca05e38507d` makes `/c` a generic flag-carried qualifier, at which point `divt/c`
matches `DIVT`+flags too. Two encodings for one spelling; the ambiguity should be
resolved (drop `DIVTC`).

---

#### `9ca05e38507d` — [Alpha] Keep the -mieee policy out of the MC layer

The design (qualifier in `MCInst` flags, like X86 prefixes) is right, the four-bullet
problem statement is accurate, and moving the `-mieee` question from `llvm-mc` to `llc`
is the correct test split. But it does not finish the job.

**BUG 1 — `cvttq/svc` cannot be disassembled.** `AlphaDisassembler.cpp`:

```cpp
if (!Alpha::fpRounds(TrapClass) && RM != Alpha::FPRoundNormal)
  return MCDisassembler::Fail;
```

`fpRounds()` is true only for TrapClass 1 and 4; `cvttq` is TrapClass 3. Under `-mieee`
the encoder produces func `0x52f` for `CVTTQ` (`0x02f | 0x500`). Decoding `0x52f`:
no table entry → fallback → `RoundBits = 0x52f & 0xc0 = 0` → `RM = FPRoundChopped` →
`TrapClass == 3` → **`Fail`**. So the one instruction the commit message singles out
("A qualified word could not be disassembled at all — including one this compiler had
just produced") is still undisassemblable. Every float→int conversion in
`-mieee` output is affected.

**BUG 2 — `cvttq/svc` assembles to the wrong instruction.** Parser path: `svc` splits
to trap `sv` (0x500) + round `c`; the retry matches base `cvttq` = `CVTTQn`
(func `0x0af`). Encoder:

```cpp
Bits |= TrapBits << 5;
if (Alpha::fpRounds(TrapClass) && RM != Alpha::FPRoundNormal)   // false for TrapClass 3
  Bits = (Bits & ~(0xc0u << 5)) | (Alpha::getFPRoundFuncBits(RM) << 5);
```

The rounding bits are dropped, so `cvttq/svc` encodes as `0x5af` = `cvttq/sv`
(round-to-nearest). Silent miscoding of a real glibc/libgcc instruction — the exact
failure mode `af082d9112d0` is titled after.

`llvm/test/MC/Alpha/fp-qualifiers.s` covers `cvttq`, `cvttq/c` and `cvttq/sv` but
carefully not `cvttq/svc`. Adding that one line to the test would have caught both bugs.

**Style.** `TSFlags & 0x7` appears as a bare magic number in four files
(`AlphaAsmParser.cpp`, `AlphaDisassembler.cpp`, `AlphaInstPrinter.cpp`,
`AlphaMCCodeEmitter.cpp`). Give it a name (`AlphaII::TrapClassMask`) in
`AlphaMCTargetDesc.h` next to the other qualifier helpers.

**Style.** `AlphaMCTargetDesc.h` now opens `namespace Alpha { … }` twice in a row
(the new qualifier block, then the existing OTS-mode block). Merge them.

---

#### `75e8a679a184` — [Alpha] Assemble .usepv, .gprel32 and the !gpdisp!N pairs

**SPLIT.** Three unrelated directives in one commit, plus a fourth undocumented change:
`.Case("lituse_jsr", Alpha::fixup_alpha_lituse_jsr)` is added to the specifier switch
(`AlphaAsmParser.cpp:600`) with no test and no mention; its test arrives six commits
later in `9f2b6d35d9e6`. Move that line there.

`llvm/test/MC/Alpha/brsgp.s` is also in the wrong commit — it tests the
`needsRelocateWithSymbol` change made in `5bb3fc961dbd`.

**BUG — `!gpdisp!<non-integer>` is silently accepted:**

```cpp
if (getLexer().is(AsmToken::Exclaim)) {
  getParser().Lex(); // !
  if (getLexer().is(AsmToken::Integer))
    GpDispSeq = getLexer().getTok().getIntVal();
  getParser().Lex(); // number
}
```

If the token is not an integer it is consumed anyway and `GpDispSeq` stays 0, so
`!gpdisp!foo` assembles as an unpaired `!gpdisp` with no diagnostic. Error instead.

**BUG — an unmatched `!gpdisp!N` produces a bogus object, not an error.** If a file
contains a first occurrence with no second, `GpDispLdaLabels` still holds an
*undefined* temp symbol at EOF and the ldah's fixup expression
`(gpdisp_lda - gpdisp_ldah)` references it. Nothing checks `GpDispLdaLabels.empty()`
at end of file. GNU as diagnoses this. No test.

**Nit.** After the second occurrence the sequence number is erased, so a third
`!gpdisp!1` in the same file silently starts a new pair rather than erroring.

**Nit.** `static_cast<MCSymbolELF *>(Sym)` in `.usepv` (and in `c947748cd5d7`) is an
unchecked downcast on a generic `MCSymbol`. Use `cast<MCSymbolELF>`.

The `.usepv` bit manipulation itself (`(getOther() & ~0x88) | Other`) is correct, and
`usepv.s`/`gpdisp.s` are good, focused tests.

---

#### `c63d11231dfb` — [Alpha] Assemble the floating-point branches and the IEEE data directives

**SPLIT.** FP branches and `.s_floating`/`.t_floating` are two unrelated features with
two separate tests (`fp-branch.s`, `float-directives.s`).

**BUG (fixed one commit later — should never have landed).** The alignment uses
`emitCodeAlignment` unconditionally, so `.t_floating` in `.data`/`.rodata` pads with
`unop` instructions. `c7fc0b19fe2d` fixes exactly this. **Squash `c7fc0b19fe2d` into
this commit.** `float-directives.s` as landed here only exercises `.text`, which is why
it did not catch it — the `DATA` prefix added by the fix is what the test needed from
the start.

**Nit.** `Value.convertFromString(getParser().getTok().getString(), …)` is handed the
raw token text for `AsmToken::Integer` too; `0x10` would go through `APFloat`'s hex
parser rather than being read as the integer 16. Untested.

The FP-branch tablegen (`FBForm` inside a `let isBranch…in`) and `fp-branch.s` are fine.

---

#### `c7fc0b19fe2d` — [Alpha] Fill .s_floating/.t_floating alignment padding to suit the section

Correct fix. **Squash into `c63d11231dfb`** — it repairs a bug introduced one commit
earlier in the same series, and its test change is the test the earlier commit was
missing. Drop `Fixes ALPHA-025.`

---

#### `e484ee623829` — [Alpha] Branch on a floating-point comparison in the float unit

Good change, well justified, and the quantified result ("1705 → 1596 instructions,
ftoit 29 → 12") plus cross-checking 12 predicates against GCC on real hardware is
exactly the evidence a reviewer wants.

**Noise.** `AlphaInstrInfo.td:1318` gains a stray blank line unrelated to the change.

**Claim to correct.** The commit message and the td comment both attribute the
`xor (setcc …), 1` form to "the branch folder … when it reverses a branch to fall
through". The branch folder runs on MIR, long after ISel; the inverted form here comes
from `SelectionDAGBuilder`/DAGCombiner. Fix the attribution or drop it.

**Repetition.** 40 near-identical `def : FBrPat<…>` / `def : FBrNotPat<…>` lines; a
`multiclass` over the ten (cc, cmp, operand-order) triples would halve it.

Tests are good — `CHECK-NEXT` on the branch and `CHECK-NOT: ftoit`/`stt`/`xor` are
non-vacuous here because each is followed by a positive `CHECK-LABEL`.

---

#### `e8d8565673d5` / `527b9b1ab286` / `960dff3ca980` — the three [MC] st_other commits

##### Ordering: these must be one commit

`960dff3ca980`'s own message is the indictment:

> That is a real behaviour change, not a theoretical one. With `.variant_cc target;
> alias = target' on riscv64, the alias came out with st_other 0x80 -- it silently
> claimed a non-standard calling convention, which a linker acts on.

`e8d8565673d5` and `527b9b1ab286` are therefore **wrong as landed**, and any bisect
landing between `e8d8565673d5` and `960dff3ca980` mis-sets `STO_RISCV_VARIANT_CC`,
`STO_AARCH64_VARIANT_PCS`, `STO_MIPS_MICROMIPS`/`STO_MIPS_PIC`, and the PPC64
local-entry field on every alias. Upstream will not take a series that knowingly breaks
four targets for two commits. **Squash all three into a single commit** that introduces
`InheritedSTOtherMask` from the outset, keeps `llvm/test/MC/ELF/alias-other.s` as the
generic negative test, and keeps the two Alpha tests as the positive ones. The result
is also a much shorter review for the generic-MC reviewers.

If they must stay separate, `960dff3ca980` has to come first (mask infrastructure,
default zero, no behaviour change) and the two propagation commits after it.

##### `e8d8565673d5` — .symver chains

* The `resolveAliasedOther` doc comment (`ELFObjectWriter.cpp:409-418`) is nine lines
  for a ten-line function, and three of them are a hedge about "attacker- or
  generator-supplied assembly would hang" for a case the comment itself says is
  already rejected upstream. Cut to two lines.
* The function is described as returning "the first target-specific st_other value" but
  before `960dff3ca980` it returns *any* non-zero `st_other`, including the STV_*
  visibility bits in 1:0. On a target where an alias names a hidden symbol, this
  returned `2` and `Alias->setOther(2)` overwrote the `setVisibility()` set two lines
  above. Fixed incidentally by the mask, which is another reason to squash.
* Test lives in `llvm/test/MC/Alpha/` although the code is generic. Keep a generic
  positive test in `llvm/test/MC/ELF/` too.

##### `527b9b1ab286` — variable-assignment chains

* Guard bug that survives into `960dff3ca980`: `writeSymbol` and
  `executePostLayoutBinding` both do

  ```cpp
  uint8_t Other = Symbol.getOther();
  if (!Other)
    Other = resolveAliasedOther(Symbol, Mask);
  ```

  The gate uses the **unmasked** `getOther()`. On a target with a non-zero mask, an
  alias that happens to carry any unrelated `st_other` bit blocks inheritance of the
  masked bits entirely. It should be `if (!(Other & Mask))`. Benign on Alpha (0x88 is
  all Alpha defines) but this is generic code and the next target to set a mask will
  hit it.
* `weak-alias-other.s` checks only `link3` from the three-hop chain; `link1`/`link2`
  and `chain_fn` are defined but never checked. Either check them or drop them.
* Commit message ends with "Add a test covering the .weak/.set pattern with
  STO_ALPHA_NOPV." — restating the diff; LLVM messages don't narrate the test hunk.

##### `960dff3ca980` — per-target mask

Correct as a design. Remaining points:

* `MaxDepth = 16` silently truncates rather than diagnosing. For a legitimate 17-link
  alias chain the alias just loses its bits with no warning. A `report_fatal_error` or
  a diagnostic would be better than a silent wrong answer, given the linker acts on it.
* `AlphaMCAsmInfo.cpp` writes the literal `0x88` with the names only in a comment
  (`// STO_ALPHA_NOPV | STO_ALPHA_STD_GPLOAD`). Define those two constants once (they
  are already open-coded as literals in `AlphaAsmParser.cpp`'s `.usepv` handler and
  in `AlphaAsmPrinter`) and use them here.
* `llvm/test/MC/ELF/alias-other.s` is the right test and its `# CHECK: Other: 0` on the
  alias is a genuine regression guard. Good.

---

#### `c947748cd5d7` — [Alpha] Set STT_FUNC on .ent symbols, matching GAS behavior

Correct and well motivated (glibc ABI-list classification).

* `static_cast<MCSymbolELF *>(CurEntSym)->setType(...)` — use `cast<MCSymbolELF>`.
* `// Mark the symbol as a function, matching GAS behavior.` restates the line below it
  *and* the commit subject. Delete.
* The commit message's second paragraph ("Also extend the weak-alias-other.s test to
  cover both .prologue 0 … and .prologue 1 …") narrates the test diff. LLVM messages
  say why, not what changed in which file.
* Ordering note: because this commit flips `NOTYPE` → `FUNC`, it has to update
  `symver-other.s`, `weak-alias-other.s` and `gnu-macros.s`. That churn disappears if
  the three `st_other` commits are squashed and this one moves ahead of them.

---

#### `09c0d33e6eac` — [Alpha] Lower switch jump tables as GP-relative 32-bit offsets

Correct approach (matches GCC PIC lowering, kills DT_TEXTREL).

* **Broken as landed**; `58230c70ee79` fixes the assembly-printing half two commits
  later. See below — squash.
* `#include "MCTargetDesc/AlphaFixupKinds.h"` is inserted above `"AlphaInstrInfo.h"`,
  breaking the sorted include block that clang-format enforces.
* `jumptable.ll`'s five bare `; RELOC: R_ALPHA_GPREL32` lines are unanchored — they
  would match GPREL32 relocations from anywhere in the file. `RELOC-COUNT-5:` or
  anchoring on the `.rela.rodata` header + `CHECK-NEXT` would be tighter.

---

#### `58230c70ee79` — [Alpha] Print jump table entries with .gprel32

**Squash into `09c0d33e6eac`.** The message says so itself: "jumptable.ll checked the
broken output; its RELOC prefix could not catch this because it only ran
-filetype=obj." Between the two commits, every `-S`, `-save-temps` and
`-fno-integrated-as` build produces a jump table of absolute addresses that the
dispatch sequence then adds `$29` to — i.e. wild branches. That is a two-commit
bisect hole for anything using a switch statement.

**Concerns if it stays separate:**

* `OutStreamer->emitRawText()` from an `AsmPrinter` override will draw fire upstream;
  it bypasses `MCStreamer` entirely. The conventional route is an `MCAsmInfo` hook or a
  target `MCStreamer` that knows how to print the expression. At minimum, note in the
  message why the `MCSpecifierExpr` route was not usable.
* The new assert in `AlphaMCAsmInfo::printSpecifierExpr`:

  ```cpp
  assert(!Name.empty() && "a specifier with no `!' spelling reached the expression printer");
  OS << " !" << Name;
  ```

  In a release build a nameless specifier now prints a bare trailing ` !` where it
  previously printed nothing — turning an assert into malformed assembly. Use
  `llvm_unreachable` inside an `if (Name.empty())`, or keep the guard.
* Drop `Fixes ALPHA-001.`

---

#### `fb38cbbc3e28` — [Alpha] Use PC-relative EH frame pointer encodings for PIC

Small, correct, target-gated change to generic code; the encodings match GCC's 0x9b/0x1b
CIE augmentation. No objections. Bulleted commit-message body is unusual for LLVM but
harmless here since it is a literal list of three assignments.

---

#### `b64b1f0a4c73` — [Alpha] Keep a misaligned store together

Real miscompile, found the right way (GCC torture under qemu), and the fix mirrors the
existing byte/word store pseudos. The expansion order (both `ldq_u`s before either
`stq_u`, high half first) is correct.

* **`Size` is wrong.** `RMW_USTORE` inherits `AlphaInst`'s default size (4) but expands
  to nine instructions (36 bytes). `getInstSizeInBytes()` returns
  `MI.getDesc().getSize()`, so any consumer that measures code size before
  `expandPostRAPseudo` — branch relaxation, `-msmall-text` decisions, inline-asm size
  accounting — under-counts by 32 bytes. Contrast `LDGPself` in `7a39491dc76a`, which
  explicitly sets `Size = 12`. (`RMW_STOREI8`/`RMW_STOREI16` look to have the same
  pre-existing problem; worth a separate fix.)
* Unrelated hunk: the `#include` reorder in `AlphaISelLowering.h:15-19`. Drop or split.
* The `OTS_CALL` enumerator move is necessary (it has to leave the
  `LAST_MEMORY_OPCODE` range) but is not mentioned in the message; one line would help.
* Test is good — the `MIR` prefix checking `:: (store (s64) into %ir.p, align 1)`
  directly targets the `getMemIntrinsicNode` half of the fix.

---

#### `c6254c100ec6` — [Alpha] Expand i128 shifts and return wide values in memory

**SPLIT (borderline).** `setOperationAction(*_PARTS, Expand)` is a two-line legality
fix; the return-ABI change (`RetCC_Alpha`, `CanLowerReturn`, `SRetReturnReg`,
`LowerFormalArguments`, `LowerReturn`) is a substantial ABI commit. The message argues
they were found together, which is true but is not an argument for one commit — the ABI
half needs its own review and its own bisect point.

**ABI concern.** `CCIfType<[f32, f64], CCAssignToReg<[F0, F1]>>` is now permissive at the
LLVM level for *any* two-float return, not just complex. `ret_two_doubles` in
`ret-in-memory.ll` demonstrates `{double, double}` coming back in `$f0/$f1`. GCC returns
a `struct { double a, b; }` **in memory** (`alpha_return_in_memory` judges by
`TYPE_SIZE`); only `_Complex double` is the exception. Clang forces sret for the struct
case via `isAggregateTypeForABI`, so C code is fine, but any other frontend or hand-
written IR gets a GCC-incompatible convention, and the test enshrines it. Either
restrict this at the LLVM level or add a comment saying the restriction lives in the
frontend and why that is acceptable.

**Nit.** `if (!Ins.empty() && Ins[0].Flags.isSRet())` assumes the sret argument is first;
that is the generic contract, but an assert would document it.

Tests are thorough (`ret-in-memory.ll`, `shift-i128.ll`, plus the `ret.ll` addition);
`shift-i128.ll`'s `ashr_i128_const` pins specific input registers (`$18`, `$17`), which
is brittle but acceptable.

---

#### `96c198333e47` — [clang][Alpha] Fix the ABI for __int128 and complex types

Correct. `isXFloating` already special-cases `ComplexType` with a 128-bit element, so
the "isXFloating above has already caught that" claim for `_Complex long double` holds
(`clang/lib/CodeGen/Targets/Alpha.cpp:47-52`). Verifying in all four cross-compiler
directions under qemu is the right level of diligence, and saying so in the message is
appropriate.

* **Test location.** `clang/test/CodeGen/alpha-scalar-abi.c` while the sibling test is
  `clang/test/CodeGen/Alpha/long-double-abi.c`. Pick one directory.
* The 6-line header comment in the test duplicates the commit message almost verbatim.
  Two lines suffice.
* `take_i128` is checked as `void @take_i128(ptr {{.*}}sret(i128){{.*}}, i128 noundef` —
  worth asserting explicitly (in a comment or a second CHECK) that the `i128` argument
  is split across `$16`/`$17` by the backend, since "still passed by value, as two
  quadwords" is a claim the IR-level check does not verify.

---

#### `8ef85f7d1945` — [clang][Alpha] Accept -mlong-double-64

Correct and minimal; `TargetInfo::adjust` handles `LongDoubleSize == 64` generically, so
no `Targets/Alpha.h` change is needed.

* The `LD64` half of `long-double-abi.c` consists of two `LD64-LABEL` lines and nothing
  else. There is no `LD64-NOT: fp128` and no `LD64` coverage of `cadd`
  (`_Complex long double`), which under `-mlong-double-64` becomes `_Complex double`
  and changes from `sret({fp128, fp128})` to `{double, double}` — the most interesting
  consequence of the flag, untested.
* `; // Already the default; no-op.` — an empty statement with a comment; prefer
  `if (…) { /* no-op: Alpha's long double is already 128-bit */ }` or invert the
  condition.

---

#### `2ecc1daa2618` — [clang][Alpha] Enable _BitInt support up to 64 bits

Good rationale, especially the `__BITINT_MAXWIDTH__` → `__SIZEOF_INT128__` fallback
argument, which is the non-obvious half.

* **Untested claim.** The commit asserts "the backend promotes every sub-i64 integer type
  to i64 without special-casing", but `clang/test/Sema/alpha-bitint.c` runs
  `-fsyntax-only`. `_BitInt(17) narrow(_BitInt(17) x) { return x + 1; }` never reaches
  ISel, so nothing verifies that a non-power-of-two `_BitInt` actually codegens. Add a
  `clang/test/CodeGen/` or `llvm/test/CodeGen/Alpha/` companion.
* `size_t getMaxBitIntWidth() const override { return getLongLongWidth(); }` — the value
  is 64 either way, but tying the `_BitInt` cap to `long long`'s width is a
  coincidence, not a relationship. `return 64;` is clearer and matches the comment.

---

#### `9f2b6d35d9e6` — [MC][Alpha] Relocate a jsr's call target the way GNU as does

Two changes of very different weight in one commit:

1. the `JSRt` assembler form + `!lituse_jsr` suffix (MC-only, new capability);
2. **emitting the lituse before the hint in codegen** — a fix that changes the
   relocations of every external call clang has ever produced for Alpha and is the
   reason GNU ld was not relaxing them.

(2) deserves its own commit and its own headline; it is buried in the fifth paragraph.
**SPLIT.**

* The `.Case("lituse_jsr", …)` parser line belongs here, not in `75e8a679a184`.
* `std::rotate(Fixups.begin() + FirstFixup, Fixups.end() - 1, Fixups.end())` is correct
  (moves the just-pushed lituse to the front of this instruction's fixups, no-op when
  it is the only one) but obscure. `Fixups.insert(Fixups.begin() + FirstFixup, …)` from
  `addLituse` would express the intent directly.
* `JSRt` declares `bits<14> sym; let hint = sym;` while `getCallTargetEncoding` returns
  0 unconditionally — the `let hint` is dead. Harmless, but it reads as if the operand
  contributes to the encoding.
* **`jsr-target.s` tests only `jsr $26, ($27), 0`.** The very next-but-one commit
  (`af082d9112d0`) says: "getCallTargetEncoding returned 0 for a plain number, so the
  hint on `jsr $26, ($27), 4' … was thrown away. The existing test only ever used 0,
  which is why it looked right." That bug is *introduced here*. Fold the fix in and use
  a non-zero hint in the test.

---

#### `af082d9112d0` — [Alpha] Stop the assembler silently miscoding the full jump forms

**SPLIT — this is five unrelated bug fixes plus a sixth change in one commit:**
`ldgp` destination/displacement, `ret` hint, `jsr`/`jmp` hint scaling, `isParenReg`
displacement rejection, the BWX feature check on the GOT-load macro, and a `pseudo-asm.s`
output change. Each has a distinct root cause, a distinct test, and a distinct
bisect value. Upstream will ask for this to be broken up.

**Squash backwards:** the `getCallTargetEncoding` fix repairs a bug introduced by
`9f2b6d35d9e6` (see above).

**Commit message.** The closing four paragraphs are private-tracker triage
("Fixes ALPHA-010 and the feature-check half of ALPHA-011. / The rest of ALPHA-011 does
not reproduce. It reports that…"). None of it belongs in an upstream message. If the
`.equ` behaviour is worth documenting, document it in a test comment.

**Verify — `ldgp` with a non-zero displacement.**

```
# CHECK: ldah $29, 4($27) !gpdisp
# CHECK: lda $29, 8($29)
	ldgp $29, 8($27)
```

The `R_ALPHA_GPDISP` relocation makes the linker write *both* halves of the pair from
the computed gp displacement, so a literal `8` sitting in the `lda` field is discarded
at link time. If GNU as folds the offset into the relocation addend instead, the byte
sequences may still match while the semantics do not. This needs to be confirmed
against `ld`, not just against `as`; the message's "byte-identical" claim does not
cover it.

**Gap.** `getCallTargetEncoding`'s new `return (uint64_t(MO.getImm()) >> 2) & 0x3fff;`
silently rounds a non-multiple-of-4 hint down and silently truncates an out-of-range
one. No diagnostic, no test.

**Test placement.** The new RUN lines in `gnu-macros.s` are appended at the *bottom* of
the file, below all the assembly. LLVM puts RUN lines at the top.

`paren-reg-forms.s` is a good test — positive encodings plus a negative `ERR` prefix for
the three rejected forms.

---

#### `4b2d145694da` — [MC][Alpha] Support the 's' section flag for SHF_ALPHA_GPREL

Correct and small; touches generic MC in three places, all properly arch-gated.

* `SHF_ALPHA_GPREL = 0x10000000` is added as a second enumerator with the same value as
  `SHF_HEX_GPREL` in the same anonymous enum. That is legal and matches how LLVM handles
  other `SHF_MASKPROC` collisions, but it is worth grouping the new constant with the
  other Alpha definitions rather than adjacent to Hexagon's, so nobody later "de-dupes"
  them.
* Inconsistency: `ENUM_ENT(SHF_ALPHA_GPREL, "s")` supplies an alt-name where the
  neighbouring `ENUM_ENT(SHF_HEX_GPREL, "")` does not. Since both mean `s`, pick one
  convention.
* `section-gprel.s` asserts the exact flag *ordering*
  (`SHF_ALLOC`, `SHF_ALPHA_GPREL`, `SHF_WRITE`) with `CHECK-NEXT`; that is fine but
  couples the test to llvm-readobj's sort order.

---

#### `85f114c81d51` — [MC][Alpha] Accept R_ALPHA_* names in .reloc

Clean, idiomatic (`ELF_RELOC` X-macro over `ELFRelocs/Alpha.def`, the same pattern other
targets use), and the message correctly explains why it is needed
(`R_ALPHA_LITUSE` is unreachable from any instruction operand, so this is the only way
to test it). Test is tight — `CHECK-NEXT` throughout, exact addends.

Only nit: `//`/`///` comment style in a `.s` file; use `#`/`##`.

### lld, compiler-rt, sanitizers, libunwind, OpenMP (commits 221–250)

#### `b8f7b798eb79` — [Alpha] Do not widen a float with cvtst

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

#### `a2cab01755e0` — [lld] Add Alpha ELF support for static linking

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

#### `949b9148e118` — [lld] Support dynamic linking for Alpha

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

#### `f69138566f06` — [lld] Implement multi-GOT for Alpha

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

#### `6164068a5336` — [lld][Alpha] Support R_ALPHA_GOTDTPREL

Correct in itself. The `GK_DtpOff` enum addition belongs in `f69138566f06` (above).

- `AlphaFixupKinds.h:35-38`: the comment block now describes both `gottprel` and
  `gotdtprel` but sits above `fixup_alpha_gottprel` only, so the new enumerator
  reads as undocumented. Split into two comments.
- `llvm/test/MC/Alpha/tls-reloc.s` checks relocation types and symbol names but no
  offsets or addends. Adequate for a specifier-parsing test.

#### `adbd0c2797f1` — [lld][Alpha] Support ifuncs with R_ALPHA_IRELATIVE

The design (IRELATIVE straight onto the GOT slot, no PLT) is sound and both failure
modes are diagnosed and tested.

- `getImplicitAddend` should be in `949b9148e118` (see above); this commit's own
  message argues the case.
- Only `R_ALPHA_REFQUAD` gets the read-only-section ifunc check. `R_ALPHA_REFLONG`
  and the `R_ALPHA_SREL*` family against an ifunc fall through to the generic path
  with no diagnostic. Either extend the check or say why they cannot occur.
- `alpha-ifunc.s` covers only the executable case. An ifunc in a shared object is
  untested.

#### `78e6313e075f` — [lld][Alpha] Diagnose !samegp against a symbol with no prologue marking

Correct, matches bfd's three-way switch, and the wording matches GNU ld.

- **Squash into `a2cab01755e0`.** That commit introduced the two-way test and the
  `alpha-branch.s` test that covers the two valid markings; this is its missing
  third arm. Nothing between the two depends on the buggy behaviour.
- **`Fixes ALPHA-022.`** — remove.
- The commit message's "alpha-branch.s already covers both valid markings; the new
  test covers the third case, which nothing did" is coverage bookkeeping; drop it
  once squashed.

#### `63073b6938b2` — [Alpha] Tag a direct tail call for relaxation

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

#### `4fa520dcb620` — [Alpha] Complete the scheduling models

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

#### `0c435020bd35` — [Alpha] Load a call's procedure value at the call

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

#### `802ced3d6201` — [Alpha] Declare the real size of the multi-instruction call pseudos

Good change; the `scope_exit` guard in `encodeInstruction` is the right mechanism.

- **Squash backwards into `0c435020bd35`** (above).
- The assert is `Declared == 0 || CB.size() - StartSize == Declared`. Since
  `MCInstrDesc::getSize()` is 0 for anything that doesn't set `Size` explicitly, a
  future multi-word pseudo that forgets `Size` is *not* caught, contrary to the
  message's "a future pseudo that grows an instruction fails on the first test that
  encodes it". Either say "a pseudo that declares a size" or make the guard also
  assert `Declared != 0` for the multi-word opcodes it knows about.
- **`Fixes ALPHA-014.`** — remove.

#### `6ec35f580082` — [lld] Implement --relax for Alpha

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

#### `a48782af2efc` — [lld] Relax Alpha tail calls

Correct: `func != FUNC_JSR && func != FUNC_JMP` correctly excludes `ret` (2) and
`jsr_coroutine` (3), and `relocate()` picks `OP_BR` vs `OP_BSR` from the original
function field while preserving Ra, giving `br $31, disp` for a jmp.

- Consider squashing into `6ec35f580082`: this rewrites the comment and condition
  that commit just added, and edits the same test's expectations. As a standalone
  commit it is defensible; as a series being prepared for upstream it is noise.

#### `35ece8bbaefa` — [lld][Alpha] Correct the HINT ordering comment and link a real hinted call

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

#### `c1d2ccf2ef73` — [lld] Relax Alpha dynamic TLS sequences

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

#### `362243ec3752` — [lld][Alpha] Keep the addend on the GOT-based TLS relocations

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

#### `7505274f0eb6` — [lld][Alpha] Explain why the relaxed TLS sequence adds $16

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

#### `6e3a2641c60c` — [lld] Drop the GOT load for an Alpha callee that never reads it

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

#### `6b83cd87ba43` — [lld] Recognize an unmarked Alpha callee that loads gp

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

#### `b2d1f72554d3` — [compiler-rt] Build the builtins for Alpha

- **`__asm__ volatile("imb")` needs a memory clobber**: `__asm__ volatile("imb" :::
  "memory")`. Without it nothing stops the compiler from sinking the stores of the
  just-written instructions past the barrier. (Several neighbouring arches in
  `clear_cache.c` have the same omission; that is a reason to note it, not to repeat
  it.)
- The commit message's second sentence runs three clauses through a nested
  em-dash aside and then tacks on "— and implement `__clear_cache` with the imb PAL
  call". Split it.

#### `87cbdaa9036b` — [libunwind] Add Alpha support

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

#### `481f0d2c90ff` — [sanitizer_common] Build and test the Alpha runtimes with -mieee

Correct and minimal. No findings. (Note the author date is identical to
`b8f7b798eb79`'s, which is harmless but suggests the two were split after the fact.)

#### `754769a43ddf` — [sanitizer_common] Walk an Alpha stack with unwind tables

- **The `message(FATAL_ERROR)` will be contentious upstream.** Hard-failing a
  configure for a whole architecture because a library choice was not made is
  heavier than anything else in `compiler-rt/CMakeLists.txt`. The escape hatch
  (`COMPILER_RT_UNWINDER_LINK_LIBS`) is thoughtfully provided, but a
  `message(WARNING)` plus disabling the sanitizers would be the more conventional
  shape. At minimum expect to defend this.
- `sanitizer_stacktrace.h`: the new `#elif SANITIZER_ALPHA` block uses `#  define`
  (two spaces) while the adjacent mips and Windows arms use `# define` (one). Match
  the neighbours.

#### `04ed9c907c5c` — [asan] Add the Alpha shadow mapping

- **The offset added here, `0x70000000000`, is unusable.** With scale 3 the shadow
  spans `[0x70000000000, 0x78000000000)`, entirely above Alpha's `TASK_SIZE` of
  `0x40000000000`; `fd2873f333a4` says so and replaces it. `alpha-shadow.ll` asserts
  `add {{.*}} 7696581394432`, i.e. the test encodes the wrong value.
  **Squash `fd2873f333a4` into this commit.**
- The comment "the existing OrShadowOffset test already selects an add rather than
  an or" is correct for both values (7<<40 and 3<<40 are not powers of two).
- `CHECK-NOT: or i64` is bounded between the `add` and `ret`, so it is a real check,
  not a vacuous one.

#### `fd2873f333a4` — [asan] Put the Alpha shadow inside the address space

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

#### `338fd6144cad` — [sanitizer_common] Say whether an Alpha fault was a read or a write

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

#### `e3bd0643c34f` — [clang][Alpha] Do not advertise MemorySanitizer

Correct, minimal, tested. The only nit is that the `CHECK-ASAN-ALPHA` prefix is
reused for the `-fsanitize=undefined` RUN line, so the name misdescribes half its
uses — `CHECK-SUPPORTED-ALPHA` would be clearer.

#### `ebccac577e35` — [OpenMP] Add support for Alpha

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

#### `f2d67b1544e8` — [lldb] Add the Alpha register contexts and ArchSpec entry

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

#### `04f0bbab0240` — [lldb] Read an Alpha ELF core file

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

### lldb, JITLink, GlobalISel, docs, and the tail cleanup commits (commits 251–278)

#### `ebc803f1d692` [lldb] Add the Alpha SysV ABI plugin

**CORRECTNESS**

- `lldb/source/Plugins/ABI/Alpha/ABISysV_alpha.cpp:104-106`:

  ```c
    if (is_signed)
      scalar.SignExtend(bit_width);
    scalar = raw;
  ```

  The `SignExtend` runs on the still-default `scalar`, then `scalar = raw`
  overwrites it. Compare `ABISysV_x86_64.cpp:201-205`,
  `ABISysV_ppc64.cpp:209-213`, `ABISysV_s390x.cpp:290-294` — all assign first,
  then sign-extend. Swap the two statements.

- `GetArgumentValues` reads stack arguments from `reg_ctx->GetSP(0)` directly.
  On Alpha the seventh and later arguments sit at CFA+0, and CFA is SP *at
  function entry*; inside a frame with a prologue the current SP is below that.
  x86_64 compensates (`sp + 8` for the return address); ppc64 adds its frame
  header. Nothing here does. Either compute from the frame's CFA or say why SP
  is correct.

- `GetReturnValueObjectImpl`:

  ```c
    if (type.IsAggregateType() || type.GetTypeInfo() & eTypeIsFloat) {
      addr_t storage_addr = reg_ctx->ReadRegisterAsUnsigned(... gpr_r0_alpha ...);
  ```

  The `eTypeIsFloat` arm is reached only for the types
  `GetReturnValueObjectSimple` rejected: complex, and `long double`. Neither is
  returned in memory on Alpha — `X_floating` and `complex float` come back in
  `$f0`/`$f1`. Reading `$0` as a pointer for those produces a garbage address.

- `RegisterIsCalleeSaved` lists `gpr_r29_alpha` ($gp). On OSF/1 the caller
  restores $gp after a call (`ldgp`); it is not preserved by the callee.
  Reporting a parent frame's $gp from the child's register is wrong. Worth a
  comment at minimum.

**TESTS** — 495 new lines, zero tests. `PrepareTrivialCall`,
`GetArgumentValues`, `RegisterIsCalleeSaved` and both unwind plans are all
untested. The sign-extension bug above would have been caught by any of them.

**LLM-TELLS** — `CreateDefaultUnwindPlan`'s comment is four lines of hedged
prose ("the best guess is the state a frame is in before its prologue has run,
which is right for a leaf frame and for the frame a signal interrupted"). The
commit message closes with "This is what lets expression evaluation call a
function in the inferior" — a summary sentence explaining the obvious.

---

#### `b291259df038` [lldb] Skip the host signal-number asserts on alpha-linux

**STRUCTURE — DELETE, fold into `6a428ab4339d`.** This commit adds a comment to
`LinuxSignals.cpp:11-15` that the very next-but-one commit rewrites in full.
Reviewers reading the series see a comment written, then replaced two commits
later, with `6a428ab4339d`'s message defensively explaining ("It is not the fix
and was never claimed to be") — which reads as an apology for having committed
`b291` first. Squash the `#if` change into `6a428ab4339d` with the final comment
text and drop this commit.

---

#### `6a428ab4339d` [lldb][Alpha] Give an alpha target the signal numbering it uses

**CORRECTNESS** — signal numbers verified against
`arch/alpha/include/uapi/asm/signal.h`: 7 EMT, 10 BUS, 12 SYS, 16 URG, 17 STOP,
18 TSTP, 19 CONT, 20 CHLD, 23 IO, 29 INFO, 30 USR1, 31 USR2. All correct. The
generic-table assertions in the unit test (7 BUS, 10 USR1, 31 SYS) are also
correct. Only `SIGBUS` among the moved signals carries non-sender codes in
`LinuxSignals.cpp`, and `AlphaLinuxSignals.cpp:74-76` re-adds them at 10.
No bug found here.

**Nit** — `AlphaLinuxSignals::AlphaLinuxSignals() : LinuxSignals() { Reset(); }`
builds the base table twice: `LinuxSignals()` already calls `Reset()` (resolving
to `LinuxSignals::Reset` during base construction), then the derived `Reset()`
calls `LinuxSignals::Reset()` again. Harmless but wasteful; the same is true of
existing `*Signals` subclasses, so it is defensible.

**STRUCTURE / sortedness** — `lldb/source/Target/UnixSignals.cpp:10-14`:

  ```c
  #include "Plugins/Process/Utility/FreeBSDSignals.h"
  #include "Plugins/Process/Utility/AlphaLinuxSignals.h"
  #include "Plugins/Process/Utility/LinuxSignals.h"
  ```

  `AlphaLinuxSignals.h` is inserted after `FreeBSDSignals.h`, out of order. This
  is exactly what `8a36984bc6a4` ("Move the insertions to their sorted
  positions") claims to have swept for, and it missed it.

**LLM-TELLS** — the message spends a whole paragraph defending a commit that
should not exist ("The host-build opt-out in LinuxSignals.cpp stays. It is not
the fix and was never claimed to be…"). This is defensive prose written to a
reviewer, not a changelog.

`LinuxSignals.h:21-23` gains `/// Protected so a port that renumbers signals can
build on this set; see AlphaLinuxSignals.` — a comment explaining an access
specifier; fine, but it restates what `protected` plus the one subclass already
say.

---

#### `a3e9049fbfed` [lldb] Add the Alpha native register context

**CORRECTNESS** — ptrace numbering checked against
`arch/alpha/kernel/ptrace.c`: $0–$29 at 0–29, usp at 30, $f0–$f30 at 32–62,
fpcr at 63, pc at 64, unique at 65. Correct. `lldb-alpha-register-enums.h` omits
`r31`/`f31`, so `GetPtraceRegNum` covers every index in the array and
`ReadAllRegisterValues` cannot hit the error path. Good.

- `NativeRegisterContextLinux_alpha.cpp:32`: `ptrace_sp_alpha = 30` is declared
  and never used — `GetPtraceRegNum` handles $30 via the `r0 + offset` range.
  Dead enumerator that `7ebcdbf4b16c` ("delete code that cannot run") missed.
- `WriteAllRegisterValues` writes `gpr_unique_alpha` (ptrace 65) unconditionally.
  `PTRACE_POKEUSER` on that slot is likely to fail on a real kernel and abort the
  whole restore.
- `PRIu32` is used at line 62 without including `<cinttypes>`.

**TESTS** — none, and none possible: the file is `#if defined(__alpha__) &&
defined(__linux__)`, so no CI configuration compiles it. Worth stating in the
message that it is compile-tested on an Alpha host only.

---

#### `47552d0f0d34` [JITLink] Add ELF/alpha support

**CORRECTNESS** — I hand-decoded the stub in `alpha.cpp:22-30` and it is right:

| bytes | word | insn |
|---|---|---|
| `00 00 60 c3` | `0xC3600000` | `br $27, .+4` |
| `0c 00 7b a7` | `0xA77B000C` | `ldq $27, 12($27)` |
| `00 00 7b a7` | `0xA77B0000` | `ldq $27, 0($27)` |
| `00 00 fb 6b` | `0x6BFB0000` | `jmp $31, ($27), 0` |

`br` writes PC+4 into $27 (= stub+4), so `12($27)` = stub+16 = the `.quad`,
which matches `B.addEdge(Pointer64, 16, ...)` and `StubEntrySize = 24`. The final
`ldq` leaves the callee's entry address in $27, which is what the callee's `ldgp`
needs. The `ELF_alpha_stubs.s` check `[31:21] = 0x61b` matches `0xC3600000 >> 21`.

Fixup expressions checked: `GPRelHigh16`'s `(Value + 0x8000) >> 16` carry-in and
`GPRelLow16`'s `(int16_t)(Value & 0xffff)` are the standard pair; `GPDisp`'s
`GP - FixupAddress` is right *given* the `ldah` is the function's entry (which
the R_ALPHA_GPDISP contract requires). No bug found.

Issues:

- `ELF_alpha.cpp:200-203` maps `R_ALPHA_BRSGP` to the same
  `BranchPCRel21ToPLT` as `R_ALPHA_BRADDR`. BRSGP is not a plain branch: it
  targets the *second* entry point (past the `ldgp`) when $gp is already correct,
  and the addend encodes the entry-point offset. If the Alpha backend never emits
  it, drop the case; if it does, this silently branches to the wrong address.
- `Pointer32`: `if (!isUInt<32>(Value) && !isInt<32>(static_cast<int64_t>(Value)))`
  accepts both a 32-bit unsigned and a sign-extended 32-bit value, i.e. it
  enforces neither range on its own. Other backends split this into
  `Pointer32`/`Pointer32Signed` precisely to avoid the ambiguity.
- No TLS relocations (`R_ALPHA_TLSGD`, `R_ALPHA_GOTTPREL`, …) are handled — they
  hit the `default:` error. The commit message lists what *is* supported but does
  not say TLS is out of scope; worth one sentence.

**TESTS** — four `.s` tests, all substantive (they compute expected values from
`section_addr`/`got_addr`/`stub_addr` rather than hard-coding, and they follow
displacements). Not vacuous. Good.

**COMMIT MESSAGE** — five paragraphs; the PPC64/TOC analogy and the stub
description belong in the header doc comments (where they already are, verbatim
— see `alpha.h:227-234` vs the message's fourth paragraph). Trim to two.

---

#### `096a635cab37` [JITLink][Alpha] Key a GOT entry on the addend, not just the symbol

**STRUCTURE — SQUASH into `47552d0f0d34`.** This fixes a bug in code introduced
two commits earlier and never shipped. Upstream will not want a broken GOT
builder followed by its fix in the same series. Fold both the `alpha.h` change
and the new test into `47552d0f0d34`.

**CORRECTNESS** — `getEntryForTargetAndAddend` uses
`std::make_pair(Target.getName(), Addend)` as the map key. `Symbol::getName()`
returns null for anonymous symbols (the code elsewhere in this same series
guards with `Sym->getName() != nullptr`). Two different anonymous targets with
the same addend will share an entry pointing at whichever was created first.
Key on `&Target` (or `Target.getAddress()`), as lld does.

**TESTS** — the new test genuinely follows each instruction's displacement to
the entry it names and checks that entry's contents, and checks entry sharing.
Good test.

---

#### `05601bb7c1e3` [Alpha] Add GlobalISel

1651 lines across 19 files, including a 571-line instruction selector, a 254-line
call lowering, and a 250-line register bank info.

**STRUCTURE — SPLIT.** The message itself argues against splitting ("so they are
not separable features"), but the natural cuts are clean and standard for a new
GlobalISel bring-up:

1. register banks + `AlphaRegisterBanks.td` + `initializeGlobalISel` wiring
   (`AlphaTargetMachine.cpp`, `CMakeLists.txt`);
2. `AlphaLegalizerInfo`;
3. `AlphaCallLowering`;
4. `AlphaInstructionSelector`.

Each is independently reviewable; today a reviewer has to hold all four in mind
at once. That the pipeline does not produce working code until all four land is
normal and is not an argument for one commit — GlobalISel bring-ups for RISCV,
M68k and CSKY all landed incrementally.

**COMMIT MESSAGE** — six paragraphs. "Two things needed care." followed by a
two-item narrative is design-doc prose. The last paragraph ("SelectionDAG remains
the default: this path is reached with -global-isel.") states the obvious. The
`isAlphaGprelAddressable lives in Alpha.h so both selectors decide alike` aside
belongs as a comment at the function, not in the log.

**TESTS** — four tests including a 170-line `global-isel.ll`. Reasonable.

---

#### `54d246db913d` [Alpha][GlobalISel] Honour safe-bwa and build-constants

**CORRECTNESS** — the reasoning is sound: selecting `RMW_STOREI8/16` while
`Predicates = [UnsafeBWStore]` says they must not appear is a genuine
predicate violation, and widening the MMO to 8 bytes matches what the DAG does.

**COMMIT MESSAGE** — the parenthetical "(The third condition the DAG checks,
that the store is at least as aligned as it is wide, was already handled:
selectLoadStore declines a misaligned access earlier.)" refers to a change that
lands *three commits later* in `d616e2b4ebde`. At this point in the series the
statement is false. Either reorder `d616e2b4ebde` before this, or drop the
parenthetical.

**LLM-TELL** — "Fixes ALPHA-028."

---

#### `2e2cf5c2b94e` [Alpha][GlobalISel] Cover the opcodes an ordinary C program produces

**COMMIT MESSAGE** — the longest in the chunk: five paragraphs plus a bulleted
"Two things this also fixes". The `verify() cannot check any of this` paragraph
is an explanation of why a test exists — that belongs as a comment at the top of
`global-isel-coverage.ll`, not in the log. The `G_SEXT_INREG is deliberately
still without a rule` paragraph is a design note that belongs next to the
(absent) rule in `AlphaLegalizerInfo.cpp`.

**STRUCTURE** — the `isFPOpcode` fix (adding `G_FSQRT`, `G_FMA`, `G_FREM`,
`G_FCOPYSIGN`, min/max) and the `{s32, p0, s32, 8}` load rule are two separate
bug fixes bundled into a coverage commit. The load-rule fix changes a test's
expected output (`global-isel-fp-copy.ll` halved its frame) — that is a
behavioural fix and deserves its own commit with its own message.

**TESTS** — `global-isel-coverage.ll` under `-global-isel-abort=1` is the right
shape for this. `global-isel-fallback.ll` documenting what falls back is good
practice.

---

#### `4026eec0de3f` [Alpha] Select a floating compare in GlobalISel

**CORRECTNESS — two real problems.**

1. **`FCMP_ORD` and `FCMP_UNO` hard-fail selection.** The legalizer rule
   (`AlphaLegalizerInfo.cpp:99-101`) is `legalFor({{s1, s32}, {s1, s64}, {s64,
   s32}, {s64, s64}})` — legality does not depend on the predicate, so *every*
   `G_FCMP` is legal. `selectFCmp`'s `default: return false;` then produces
   "cannot select" for `ord`/`uno`, which are ordinary C (`isnan(x)`,
   `x != x`). Confirmed absent everywhere: `grep -rn "FCMP_ORD\|FCMP_UNO"
   llvm/lib/Target/Alpha/` returns nothing, and neither predicate appears in
   `global-isel-fcmp.ll`, `global-isel-coverage.ll` or `global-isel-fallback.ll`.
   Either lower them (`ord x,y` = `cmpteq x,x && cmpteq y,y`) or mark them
   unsupported so the function falls back.

2. **NaN traps.** Both the selector comment
   (`AlphaInstructionSelector.cpp:401-410`) and the commit message state "a
   compare against a NaN is false". `CMPTEQ`/`CMPTLT`/`CMPTLE` without `/SU`
   raise Invalid Operation on a quiet NaN. `6ab5e7a4907f`'s own message
   ("comparing it then took a floating-point trap") is direct evidence that this
   target does trap. If the SelectionDAG path uses the same bare forms, that is a
   pre-existing decision, but the comment as written is not true and should not
   be the stated justification for the `UNE`/`UGE`/`UGT`/`ULT`/`ULE` inversions.

The ten predicate mappings that *are* implemented are individually correct
(`UGE = !OLT`, `UGT = !OLE`, `ULT = !(swap OGE) = !(swap CMPTLE)`,
`ULE = !(swap OGT) = !(swap CMPTLT)`, `UNE = !OEQ`), and `>> 62` on a 2.0
`T_floating` bit pattern (`0x4000000000000000`) is 1. Verified.

**LLM-TELLS** — the same explanation appears three times: the commit message
(paras 1–2), the function comment at `AlphaInstructionSelector.cpp:401-404`, and
the test-file header at `global-isel-fcmp.ll:3-6`, nearly word for word. This is
exactly what `cbee60b78c50` ("Keep each explanation in one place") claims to have
swept, and it did not touch this file.

---

#### `d616e2b4ebde` [Alpha] Leave a misaligned access to SelectionDAG in GlobalISel

**CORRECTNESS** — `MMO.getAlign().value() * 8 < Size` is the right test and the
right place. No issue.

**TESTS** — `global-isel-misaligned.ll` runs with `-global-isel-abort=0`, so it
checks the fallback output. If the guard is removed the `ldq_u`/`stq_u` pair
disappears and the test fails, so it is not vacuous. Good.

**STRUCTURE** — see `54d246db913d`: that commit's parenthetical already claims
this behaviour, so this should land *before* it.

**COMMIT MESSAGE** — good: short, states the failing case concretely
("memset of two bytes at offset 7 wrote one of them").

---

#### `e0b86ece3314` [Alpha] Select a constant in GlobalISel
#### `bf12fc16e596` [Alpha] Select a select in GlobalISel
#### `85cfb499e2af` [Alpha] Convert between integer and floating values in GlobalISel

These three are the best-shaped commits in the chunk: one capability each,
legalizer rule + bank mapping + selection together, a test each, and a
three-to-five-line message that explains the hardware constraint rather than the
diff. No correctness issues found.

One note on `85cfb499e2af`: "Add the legalizer rules, the register bank mappings
and the selection together, so the conversions are selectable as of this commit"
— this closing sentence appears verbatim in `bf12fc16e596` too. It is a
justification aimed at a reviewer who might ask for a split, not information; drop
it from both.

---

#### `4a55942fe7fa` [Alpha] Mask the low bit when narrowing to a boolean in GlobalISel

**CORRECTNESS** — the fix is right and the placement (at the `G_TRUNC` to `s1`)
is the correct choke point.

**COMMIT MESSAGE** — the last paragraph, "With this the gcc c-torture execute
suite through GlobalISel matches SelectionDAG exactly: 1595 pass, 19 fail, 79
unbuilt", is good evidence and worth keeping. The first two paragraphs are then
repeated almost verbatim as a 7-line comment at
`AlphaInstructionSelector.cpp:794-800` *and* again as a 4-line header in
`global-isel-bool.ll:3-6`. Same triplication as `4026eec0de3f`.

---

#### `b12c5aca8d46` [SelectionDAG] Pass the value to ShouldShrinkFPConstant

**STRUCTURE** — correctly split out as a target-independent prerequisite. This
one is right.

**CORRECTNESS / API design**

- The doc comment at `TargetLowering.h:1928-1931` says "instruction selection
  should shrink the FP constant \p Val of the specified type". That is
  misleading: the `EVT` is the *original* type (`OrigVT`) while `Val` is the
  value *as it would be stored in the smaller type* (`SVal`). Callers overriding
  this need to know that, and none of the four existing overrides document which
  is which. Say it explicitly.
- `LegalizeDAG.cpp:352-358` computes `SVal` at the top of the loop body,
  unconditionally, before the three cheap predicates that usually fail. Move it
  inside, or make it lazy.
- The four existing overrides all name the new parameter `Val` and never use it.
  LLVM convention is to leave an unused parameter unnamed (`const APFloat &`) or
  comment it (`const APFloat & /*Val*/`).

**TESTS** — no test for the common-code change on its own, which is defensible
for a signature-only refactor; the message correctly asserts no target changes
behaviour.

---

#### `6ab5e7a4907f` [Alpha] Do not shrink a float constant into a denormal

**CORRECTNESS** — `return !Val.isDenormal();` is right: `APFloat::isDenormal()`
is false for zero, so zero still shrinks, matching the comment. Inf and NaN also
survive the S→T exponent mapping and still shrink. Good.

**COMMIT MESSAGE** — the best in the chunk: names the mechanism, names the
observed failure (`__FLT_MIN__ / 2` reading back "a few hundred orders of
magnitude away"), and names the test that regressed
(`gcc.c-torture/execute/pr23941.c`, SIGFPE at -O2).

**TESTS** — `fp-constant.ll`'s `adddenormal` has a real positive check
(`ldt $f0, {{.*}}!gprellow`), so removing the fix breaks it. The `CHECK-NOT: lds`
is redundant belt-and-braces but harmless.

---

#### `440f811fc7d4` [docs] Announce the Alpha backend and list its references

**CORRECTNESS — both insertions are misplaced.**

- `CompilerWriterInfo.md`: `### Alpha` is inserted *before* `### AArch64 & ARM`.
  The Hardware section is alphabetical; AArch64 sorts before Alpha.
- `ReleaseNotes.md`: `### Changes to the Alpha Backend` is inserted between
  `### Changes to the ARM Backend` and `### Changes to the AVR Backend`.
  Alphabetically Alpha belongs before AMDGPU, not after ARM.

`8a36984bc6a4` ("Move the insertions to their sorted positions") was supposed to
catch exactly this class of error and swept neither file.

- The `Calling Standard for Alpha Systems` link is filed under
  `## ABI → ### Linux`. That document is the OSF/1 and Tru64 calling standard,
  not a Linux one. It needs its own `### Alpha` subsection under ABI, or a note.
- `https://download.majix.org/dec/alpha_arch_ref.pdf` is a personal mirror.
  Reviewers will ask for it to be dropped in favour of the bitsavers entries,
  which are already listed and cover the same document.

---

### The tail cleanup commits

`99d967450c2a` through `4790ba4c5343` are ten commits that sweep the entire
278-commit series. `git show --stat` confirms their reach: `c9a6ae54bb9d` touches
23+ files across clang, compiler-rt, libunwind, lld, lldb and llvm;
`4790ba4c5343` touches 42 test files.

**They should not ship as commits.** Every hunk in nine of the ten is a defect in
a commit earlier in the same unpublished series. Landing "here is the code" then
"here is the code formatted" then "here are the comments removed" is not how a
series is presented; it advertises that the series was written in bulk and
audited afterwards. The subjects make it worse by *counting* — "Correct four
rationales", "Put the values back into six tests", "Give four tests something to
assert", "Check the ordering and the sign extension these two tests left open".
LLVM subjects name what changed, not how many of them there were; a subject with
a numeral in it reads like an audit checklist item, which is what these are.

Per-commit disposition:

| commit | disposition |
|---|---|
| `99d967450c2a` | **SPLIT.** Contains a real behavioural fix. |
| `a0b52b0bc228` | Fold each test hunk into the commit that added that test. |
| `8364a194250d` | Fold into the `unaligned.ll` and `atomic-subword-nobwx.ll` commits. |
| `c009f8859dab` | Fold each new test into the commit whose code it covers. |
| `7ebcdbf4b16c` | **SPLIT.** Mixed comment fixes, dead-code deletion and one out-of-tree file. |
| `c9a6ae54bb9d` | Fold entirely; nothing should be unformatted in the first place. |
| `8a36984bc6a4` | Fold entirely; and it is incomplete (see below). |
| `cbee60b78c50` | Fold entirely; and it is incomplete (see below). |
| `aa91963c93a0` | Fold entirely. |
| `4790ba4c5343` | Fold entirely, except one CHECK fix (see below). |

#### `99d967450c2a` — contains a real fix that must survive as its own commit

Subject: "Give four tests something to assert, and fix what one of them found".

The `llvm/lib/Target/Alpha/GISel/AlphaRegisterBankInfo.cpp` hunk (+21/-1) teaches
`definedByFP` to look through a phi, with a `SmallPtrSet` guard against the
self-reference a loop phi makes. That is a codegen change: without it a
double that only ever comes out of a phi is stored via `stt` to a frame slot,
`ldq`'d back and `stq`'d out, and the function needs a 16-byte frame it otherwise
would not. It must be its own commit — `[Alpha][GlobalISel] Look through a phi
when deciding a value's bank`, with `global-isel-phi-bank.ll`'s two new CHECK
blocks — or be squashed into `05601bb7c1e3` which introduced `definedByFP`.

The other four hunks belong in the commits that added those tests:
`eh-regs.ll`, `bswap.ll`, `blockaddress.ll`.

Note also that this commit leaves a **duplicated explanation** it introduced, at
`global-isel-phi-bank.ll:28-34`:

```
; The result is only ever stored, so nothing downstream marks the phi as
; floating; the incoming values are what decide it.
; Here nothing downstream is floating -- the value is only stored -- so the
; incoming values are what decide it.  Getting that wrong puts the phi in one
; bank and the store in the other, ...
```

Two sentences saying the same thing, back to back. `cbee60b78c50` ("Keep each
explanation in one place") ran *after* this commit and did not remove it. Still
present at HEAD.

#### `a0b52b0bc228`, `8364a194250d`, `c009f8859dab` — test repairs

These three are genuinely valuable work — the descriptions of what was wrong are
specific and correct (a `CHECK` for `.cfi_offset $26,` with no offset does assert
nothing; a `CHECK-NOT: sextb` cannot detect a *missing* sign extension; a
one-`stq_u` order-independent check on a two-RMW sequence says nothing about
which store is last). But they are repairs to tests that shipped broken earlier
in the same series. Every hunk maps to exactly one earlier commit:

- `cfi.ll` → the CFI commit; `call-indirect.ll` → the indirect-call commit;
  `stack-args.ll` → the stack-argument commit; `build-constants.ll` →
  `-mbuild-constants`; `global-isel-constant.ll` → `e0b86ece3314` (in this
  chunk); `large-frame.ll` → the large-frame commit.
- `unaligned.ll` → the unaligned-access commit; `atomic-subword-nobwx.ll` → the
  subword-atomics commit.
- `c009f8859dab`'s new files split cleanly: `branch-cc.ll` → the BR_CC commit,
  `tls-address-folds.ll` → the TLS fold commit, `rotate.ll` → the rotate
  expansion commit (whose message admits it had *no test at all*),
  `half.ll` → the fp16 commit, `ELF_alpha_reloc_srel.s` → `47552d0f0d34` in this
  chunk, `alpha-varargs.c` → the clang varargs commit, `jumptable.ll` → the jump
  table commit.

None of them changes behaviour, so none needs to survive as its own commit.

`c009f8859dab`'s message ends with "A jump table under -relocation-model=pic,
which needs no second form because the table is gp-relative either way. Worth
stating rather than assuming." — that last sentence is the author reasoning aloud
to a reviewer. Drop it.

#### `7ebcdbf4b16c` — SPLIT; contains one file that does not belong in an [Alpha] commit

Subject: "Correct four rationales and delete code that cannot run" — thirteen
independent changes under one subject.

**Wrong tag:** `llvm/lib/MC/MCSymbolELF.cpp` is not Alpha. Rewriting a
`static_assert`'s comment in generic MC code under an `[Alpha]` subject will get
the whole commit bounced. It needs its own `[MC]` commit or to be folded into
whichever earlier commit added that assert.

**A real defect introduced:** removing `AlphaISD::THREAD_POINTER` from the enum
left its comment orphaned. `AlphaISelLowering.h:92-96` now reads:

```c
  BR_GE,

  // The thread pointer, read from the PALcode unique value (call_pal rduniq).

  // Thread-pointer-relative address parts for local-exec TLS, materialized
  // with ldah !tprelhi and lda !tprello.
```

A doc comment for a deleted enumerator, followed by a blank line, followed by the
next enumerator's comment. Delete it.

**One arithmetic claim in the message is wrong.** For the `V / F >= 3` guard the
message says "the smallest survivor divisible by 9 is 27, by 5 is 15, by 3 is
15". 15 is not a survivor — `isPowerOf2_64(Odd + 1)` returns early for 15 (16 is
a power of two). The actual smallest survivors are 21 for F=3 and 25 for F=5.
The conclusion (the guard cannot fail) still holds, since `V >= Odd` and
21/3 = 7, 25/5 = 5, 27/9 = 3 are all ≥ 3 — but the stated proof is wrong, and it
is the whole justification for removing a guard from a DAG combine. Nothing in
the test suite covers the removal.

**A silent robustness regression.** Deleting the `BitSize == 0` check in
`AlphaTargetTransformInfo.h:getIntImmCost` relies on the `assert(Ty->isIntegerTy())`
above it — but asserts are compiled out in Release. In a Release build a
non-integer `Ty` now falls through to the sizing code with `BitSize == 0` instead
of returning `TCC_Free`. The assert does not make the guard unreachable; it makes
it unreachable *in +Asserts builds only*.

**A stale comment the pass missed.** `AlphaFrameLowering.cpp:26-27` still says
"Amount must fit in the 16-bit signed displacement; larger frames are not handled
yet" — directly above code that handles the 32-bit case and `report_fatal_error`s
past 2 GiB. A pass whose subject is "correct four rationales" should have caught
the one contradicted by the function it is editing.

**The message's closing paragraph** — "Two things the review lists here are not
defects. CMOV_cc's `func' parameter is used…" — is a reply to a review comment.
It has no place in an upstream commit log; it reveals that the commit exists to
close out an audit.

Recommended: split into (a) an `[MC]` commit for `MCSymbolELF.cpp`, (b) squash
each comment correction into the commit that wrote the comment, (c) squash each
dead-code deletion into the commit that added the dead code, (d) drop the
`getIntImmCost` change, (e) fix the orphaned comment.

#### `c9a6ae54bb9d` — clang-format

Nothing should ever have been unformatted. Fold every hunk into its originating
commit and run `git clang-format` per-commit instead. The four "spots it cannot
reach" the message lists are separate bugs and each belongs in its own home:

- the `/*Setup=*/` comment naming a parameter that does not exist
  (`skipFrameInstrs` takes `IsSetup`) is a real incorrect-comment bug, not a
  formatting one;
- the f128 `FCOPYSIGN` `_` binding likewise;
- the `LinuxSignals.cpp` `#endif` shortening lands in a file this chunk already
  touches twice (`b291259df038`, `6a428ab4339d`) — a third edit to the same three
  lines across the series.

#### `8a36984bc6a4` — sorted positions; incomplete

Six real out-of-order insertions found and fixed. But the sweep missed at least
three, all in this chunk:

- `lldb/source/Target/UnixSignals.cpp:11` — `AlphaLinuxSignals.h` after
  `FreeBSDSignals.h` (added by `6a428ab4339d`, which this commit post-dates).
- `llvm/docs/CompilerWriterInfo.md` — `### Alpha` before `### AArch64 & ARM`.
- `llvm/docs/ReleaseNotes.md` — Alpha backend section after ARM.

An incomplete sweep is worse than no sweep: it establishes that the author knows
the lists are sorted, and then leaves three violations. Fold the hunks into their
originating commits and there is nothing to be incomplete about.

#### `cbee60b78c50` — one explanation per place; incomplete

Five rationales de-duplicated. The sweep missed:

- `global-isel-phi-bank.ll:28-34`, two consecutive sentences saying the same
  thing (introduced by `99d967450c2a`, which this commit post-dates — see above).
- `4026eec0de3f`: the fcmp rationale appears in the commit message, in
  `AlphaInstructionSelector.cpp:401-404`, and in `global-isel-fcmp.ll:3-6`.
- `4a55942fe7fa`: the boolean-narrowing rationale appears in the commit message,
  in `AlphaInstructionSelector.cpp:794-800`, and in `global-isel-bool.ll:3-6`.

Both of those are commits in this chunk, i.e. within the sweep's declared reach.

#### `aa91963c93a0` — comments restating the code

16 lines removed across 6 files. Uncontroversial and correct as far as it goes.
The remaining prose in the Alpha tree is clean on the classic markers — I grepped
all 461 changed files for `Note that`, `Importantly`, `This ensures`,
`In other words`, `Let's`, `In summary`, `for clarity` and found zero hits. The
de-LLM-ification did land. Fold each hunk into its originating commit.

#### `4790ba4c5343` — tutorial preambles and test names

42 test files. Two observations:

- **One hunk is not a rename.** `cix-count.ll`'s `NOCIX-NOT: ctpop` became
  `NOCIX-NOT: ctpop $`. That is required *because* the function was renamed to
  `ctpop_i64` — the old `CHECK-NOT` would now match the label line. So the rename
  and the check fix are coupled, and folding them separately into different
  commits is not possible; they must move together into the CIX commit.
- The renames themselves are right: `pc`/`lz`/`tz`/`pc32` →
  `ctpop_i64`/`ctlz_i64`/…, `f1`/`f2`/`f3` → `outline_a`/`outline_b`/`outline_c`.
  Generic single-letter test function names are a genuine tell and worth fixing —
  in the commits that wrote them.

Some of the deleted preambles were also *stale*, not merely tutorial: `imm.ll`'s
"Only immediates that fit in the 16-bit signed displacement are handled for now;
wider constants are not yet supported" was contradicted by `imm32.ll` and
`imm64.ll` sitting next to it. That is a correctness-of-documentation fix, and
noting it in the message would be more useful than the count in the subject.
