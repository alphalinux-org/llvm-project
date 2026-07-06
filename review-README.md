# Alpha backend series — review index

Reviewed range: `0bf3638..HEAD` on branch `alpha-triple-0bf3638`.
278 commits, 461 files, +29755/−88.

Reviewed by eleven passes: ten chunk reviewers over contiguous commit ranges,
plus one whole-series structural pass. Findings marked **verified** were
re-checked directly against the tree; everything else is a chunk reviewer's
report and should be confirmed before acting.

## Files

| File | Commits (oldest-first) | Area |
|---|---|---|
| `review-series-level.md` | all 278 | structure, ordering, squash/split, series-wide tells |
| `review-chunk-01.md` | 1–11 | generic prereqs: TailCallElim, TargetParser, BinaryFormat, MC, clang |
| `review-chunk-02.md` | 12–40 | backend skeleton, first-wave ISel |
| `review-chunk-03.md` | 41–69 | stack args, atomics, varargs, division, jump tables, CIX/FIX |
| `review-chunk-04.md` | 70–100 | MC layer, disassembler, asm parser, clang target/ABI, TLS, CFI |
| `review-chunk-05.md` | 101–130 | CFI, intrinsics/builtins, misaligned access, scheduling models |
| `review-chunk-06.md` | 131–160 | TTI, tail calls, outliner, branch relaxation, asm directives |
| `review-chunk-07.md` | 161–190 | GP addressing, trap modes, f128 via OTS, long double |
| `review-chunk-08.md` | 191–220 | EH, fixups, assembler directives, st_other MC work, i128 |
| `review-chunk-09.md` | 221–250 | lld (static/dynamic/multi-GOT/relax), compiler-rt, sanitizers, lldb |
| `review-chunk-10.md` | 251–278 | lldb, JITLink, GlobalISel, docs, and the ten tail cleanup commits |

Map a position to a hash with:

```sh
git log --reverse --format='%h %s' 0bf3638..HEAD | nl -ba | sed -n '<N>p'
```

## Start here

Read `review-series-level.md` first. It is ordered by priority and its item 0 is
a blocker.

### Blockers

1. **The series does not compile at `f69138566f06`** (verified) — `GK_DtpOff` is
   used there but only defined in the next commit. `review-series-level.md` §0.
2. **A second bisect break at `beb2e35cdf43`** — it adds a `-mlong-double-128`
   "no error" check one commit before `0443b4690da9` makes it stop erroring.
   `review-chunk-07.md`.

No commit-by-commit build was run as part of this review, so there may be more
of these. Gate the whole range before doing anything else.

### Structural work, before any per-commit edits

These move hunks between commits, so do them first:

3. **Squash the ten tail cleanup commits** (positions 269–278) into the commits
   that introduced the code they repair. `review-series-level.md` §1, and
   `review-chunk-10.md` for the two hunks that must survive as their own
   commits.
4. **Fix the clang ABI ordering** — it is landed wrong at positions 82–86 and
   corrected at 176/181/214. `review-series-level.md` §3.
5. **Strip 32 private tracker IDs** (`Fixes ALPHA-017`, `ALPHA-T03`, …) and
   **strip the 20–23 trailing blank lines present on all 278 messages**.
   `review-series-level.md` §7 and §11. Both are mechanical; do them in the same
   rebase as the squashes.
6. **Consider splitting the series** into generic prerequisites / backend /
   consumers for upstream review. `review-series-level.md` §5.

### Highest-severity correctness findings

Each is reported in the chunk file named; the three marked verified were
re-checked here.

- f128 comparisons through OTS are wrong for every NaN input — the OTS routines
  return −1/0/1 and the code treats the result as a boolean. `review-chunk-07.md`.
- `cmpxchg i32` miscompiles then fails to select for ~75 commits mid-series
  (**verified**; correct at HEAD). `review-series-level.md` §9.
- Five CMP function codes are placeholders for 51 commits, silently corrected
  under a "No functional change" claim (**verified**).
  `review-series-level.md` §8.
- Residual `!Other` mask bug in generic `ELFObjectWriter.cpp` — affects every
  ELF target (**verified**). `review-series-level.md` §12.
- Sub-word CAS reports a wrong success flag for negative expected values.
  `review-chunk-03.md`.
- `jmp`/`ret`/`jsr` operand handling drops the link register and the hint in
  several assembler paths. `review-chunk-05.md`, `review-chunk-06.md`,
  `review-chunk-08.md`.
- lld relaxation: the `+8` gp-load skip is unsound with a non-zero literal
  addend, and `startsWithGpLoad`'s ordering invariant can be violated by
  `relaxTlsCall`. `review-chunk-09.md`.
- lldb `ABISysV_alpha` sign-extends before assigning, dropping signedness.
  `review-chunk-10.md`.

### Recurring theme: tests that cannot fail

Every chunk found some. The pattern is a test whose header comment claims
coverage its CHECK lines do not provide — vacuous `CHECK-NOT` before the first
positive `CHECK`, a lone `CHECK-LABEL`, wildcarded immediates, or prefixes that
match by accident. The tail commits 269–272 were an attempt to sweep this up and
did not finish (`review-chunk-10.md` lists what they missed). Worth a dedicated
pass rather than commit-by-commit fixes.

### On LLM-tells specifically

The obvious markers are gone — a grep for `Note that|Importantly|This ensures|
In other words|Essentially|obviously|Basically` across every added line in all
461 files returns two hits, both legitimate statements about GNU as behaviour.
Commits 276–278 did that sweep.

What remains is structural rather than lexical, and is more revealing:

- 20–23 trailing blank lines on all 278 messages (**verified**) — a generation
  artifact, not something a person types.
- The existence and shape of the tail cleanup commits: a whole-series
  clang-format pass, a "move the insertions to their sorted positions" pass, a
  "drop comments that restate the code" pass.
- Subjects that count things — "Correct four rationales", "Put the values back
  into six tests", "Give four tests something to assert".
- 32 messages closing on an internal tracker queue (`ALPHA-001`..`ALPHA-030`,
  `ALPHA-T01`..`ALPHA-T07`, `ALPHA-L03`..`ALPHA-L06`), with phrases like "the
  ext-free.ll half of ALPHA-T02" that describe commits carved to close tickets.
- A `???` lab note left in `AlphaSchedule.td:405` (**verified**).
- Commit messages that restate the diff, or that contradict it — several chunks
  found messages describing code the commit does not contain, or claiming
  behaviour added several commits later.

Fixing the first four is mostly mechanical and folds into the rebase above.
