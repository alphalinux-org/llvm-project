# Review — chunk 10 (commits 251..278 of the Alpha series)

Range: `0bf3638..HEAD`, commits 251–278 oldest-first.

## Summary

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

## `ebc803f1d692` [lldb] Add the Alpha SysV ABI plugin

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

## `b291259df038` [lldb] Skip the host signal-number asserts on alpha-linux

**STRUCTURE — DELETE, fold into `6a428ab4339d`.** This commit adds a comment to
`LinuxSignals.cpp:11-15` that the very next-but-one commit rewrites in full.
Reviewers reading the series see a comment written, then replaced two commits
later, with `6a428ab4339d`'s message defensively explaining ("It is not the fix
and was never claimed to be") — which reads as an apology for having committed
`b291` first. Squash the `#if` change into `6a428ab4339d` with the final comment
text and drop this commit.

---

## `6a428ab4339d` [lldb][Alpha] Give an alpha target the signal numbering it uses

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

## `a3e9049fbfed` [lldb] Add the Alpha native register context

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

## `47552d0f0d34` [JITLink] Add ELF/alpha support

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

## `096a635cab37` [JITLink][Alpha] Key a GOT entry on the addend, not just the symbol

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

## `05601bb7c1e3` [Alpha] Add GlobalISel

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

## `54d246db913d` [Alpha][GlobalISel] Honour safe-bwa and build-constants

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

## `2e2cf5c2b94e` [Alpha][GlobalISel] Cover the opcodes an ordinary C program produces

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

## `4026eec0de3f` [Alpha] Select a floating compare in GlobalISel

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

## `d616e2b4ebde` [Alpha] Leave a misaligned access to SelectionDAG in GlobalISel

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

## `e0b86ece3314` [Alpha] Select a constant in GlobalISel
## `bf12fc16e596` [Alpha] Select a select in GlobalISel
## `85cfb499e2af` [Alpha] Convert between integer and floating values in GlobalISel

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

## `4a55942fe7fa` [Alpha] Mask the low bit when narrowing to a boolean in GlobalISel

**CORRECTNESS** — the fix is right and the placement (at the `G_TRUNC` to `s1`)
is the correct choke point.

**COMMIT MESSAGE** — the last paragraph, "With this the gcc c-torture execute
suite through GlobalISel matches SelectionDAG exactly: 1595 pass, 19 fail, 79
unbuilt", is good evidence and worth keeping. The first two paragraphs are then
repeated almost verbatim as a 7-line comment at
`AlphaInstructionSelector.cpp:794-800` *and* again as a 4-line header in
`global-isel-bool.ll:3-6`. Same triplication as `4026eec0de3f`.

---

## `b12c5aca8d46` [SelectionDAG] Pass the value to ShouldShrinkFPConstant

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

## `6ab5e7a4907f` [Alpha] Do not shrink a float constant into a denormal

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

## `440f811fc7d4` [docs] Announce the Alpha backend and list its references

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

# The tail cleanup commits

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

## `99d967450c2a` — contains a real fix that must survive as its own commit

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

## `a0b52b0bc228`, `8364a194250d`, `c009f8859dab` — test repairs

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

## `7ebcdbf4b16c` — SPLIT; contains one file that does not belong in an [Alpha] commit

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

## `c9a6ae54bb9d` — clang-format

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

## `8a36984bc6a4` — sorted positions; incomplete

Six real out-of-order insertions found and fixed. But the sweep missed at least
three, all in this chunk:

- `lldb/source/Target/UnixSignals.cpp:11` — `AlphaLinuxSignals.h` after
  `FreeBSDSignals.h` (added by `6a428ab4339d`, which this commit post-dates).
- `llvm/docs/CompilerWriterInfo.md` — `### Alpha` before `### AArch64 & ARM`.
- `llvm/docs/ReleaseNotes.md` — Alpha backend section after ARM.

An incomplete sweep is worse than no sweep: it establishes that the author knows
the lists are sorted, and then leaves three violations. Fold the hunks into their
originating commits and there is nothing to be incomplete about.

## `cbee60b78c50` — one explanation per place; incomplete

Five rationales de-duplicated. The sweep missed:

- `global-isel-phi-bank.ll:28-34`, two consecutive sentences saying the same
  thing (introduced by `99d967450c2a`, which this commit post-dates — see above).
- `4026eec0de3f`: the fcmp rationale appears in the commit message, in
  `AlphaInstructionSelector.cpp:401-404`, and in `global-isel-fcmp.ll:3-6`.
- `4a55942fe7fa`: the boolean-narrowing rationale appears in the commit message,
  in `AlphaInstructionSelector.cpp:794-800`, and in `global-isel-bool.ll:3-6`.

Both of those are commits in this chunk, i.e. within the sweep's declared reach.

## `aa91963c93a0` — comments restating the code

16 lines removed across 6 files. Uncontroversial and correct as far as it goes.
The remaining prose in the Alpha tree is clean on the classic markers — I grepped
all 461 changed files for `Note that`, `Importantly`, `This ensures`,
`In other words`, `Let's`, `In summary`, `for clarity` and found zero hits. The
de-LLM-ification did land. Fold each hunk into its originating commit.

## `4790ba4c5343` — tutorial preambles and test names

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
