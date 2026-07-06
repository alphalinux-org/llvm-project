# Review: commits 191–220 of the Alpha series (`0bf3638..HEAD`)

## Summary

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

## `acf0a0870278` — [Alpha] Take the ISA names and their meanings from GNU as

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

## `1e51a0676963` — [Alpha] Deliver the exception pointer and selector in $a0 and $a1

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

## `7a39491dc76a` — [Alpha] Reload the global pointer at a landing pad

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

## `0a630862b917` — [Alpha] Convert between f32 and f128

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

## `dd2e8a4e012e` — [Alpha] Answer the remaining f128 condition codes

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

## `3647f8a10e60` — [Alpha] Keep an f128 conversion out of the memory access next to it

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

## `5bb3fc961dbd` — [Alpha] Add R_ALPHA_GPREL16 and R_ALPHA_GPREL32 fixup support

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

## `5ce9c6144c32` — [Alpha] Encode an immediate that is not a relocation but not a constant either

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

## `bd5c29e1b543` — [Alpha] Assemble floating-point qualifier suffixes

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

## `9ca05e38507d` — [Alpha] Keep the -mieee policy out of the MC layer

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

## `75e8a679a184` — [Alpha] Assemble .usepv, .gprel32 and the !gpdisp!N pairs

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

## `c63d11231dfb` — [Alpha] Assemble the floating-point branches and the IEEE data directives

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

## `c7fc0b19fe2d` — [Alpha] Fill .s_floating/.t_floating alignment padding to suit the section

Correct fix. **Squash into `c63d11231dfb`** — it repairs a bug introduced one commit
earlier in the same series, and its test change is the test the earlier commit was
missing. Drop `Fixes ALPHA-025.`

---

## `e484ee623829` — [Alpha] Branch on a floating-point comparison in the float unit

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

## `e8d8565673d5` / `527b9b1ab286` / `960dff3ca980` — the three [MC] st_other commits

### Ordering: these must be one commit

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

### `e8d8565673d5` — .symver chains

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

### `527b9b1ab286` — variable-assignment chains

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

### `960dff3ca980` — per-target mask

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

## `c947748cd5d7` — [Alpha] Set STT_FUNC on .ent symbols, matching GAS behavior

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

## `09c0d33e6eac` — [Alpha] Lower switch jump tables as GP-relative 32-bit offsets

Correct approach (matches GCC PIC lowering, kills DT_TEXTREL).

* **Broken as landed**; `58230c70ee79` fixes the assembly-printing half two commits
  later. See below — squash.
* `#include "MCTargetDesc/AlphaFixupKinds.h"` is inserted above `"AlphaInstrInfo.h"`,
  breaking the sorted include block that clang-format enforces.
* `jumptable.ll`'s five bare `; RELOC: R_ALPHA_GPREL32` lines are unanchored — they
  would match GPREL32 relocations from anywhere in the file. `RELOC-COUNT-5:` or
  anchoring on the `.rela.rodata` header + `CHECK-NEXT` would be tighter.

---

## `58230c70ee79` — [Alpha] Print jump table entries with .gprel32

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

## `fb38cbbc3e28` — [Alpha] Use PC-relative EH frame pointer encodings for PIC

Small, correct, target-gated change to generic code; the encodings match GCC's 0x9b/0x1b
CIE augmentation. No objections. Bulleted commit-message body is unusual for LLVM but
harmless here since it is a literal list of three assignments.

---

## `b64b1f0a4c73` — [Alpha] Keep a misaligned store together

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

## `c6254c100ec6` — [Alpha] Expand i128 shifts and return wide values in memory

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

## `96c198333e47` — [clang][Alpha] Fix the ABI for __int128 and complex types

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

## `8ef85f7d1945` — [clang][Alpha] Accept -mlong-double-64

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

## `2ecc1daa2618` — [clang][Alpha] Enable _BitInt support up to 64 bits

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

## `9f2b6d35d9e6` — [MC][Alpha] Relocate a jsr's call target the way GNU as does

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

## `af082d9112d0` — [Alpha] Stop the assembler silently miscoding the full jump forms

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

## `4b2d145694da` — [MC][Alpha] Support the 's' section flag for SHF_ALPHA_GPREL

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

## `85f114c81d51` — [MC][Alpha] Accept R_ALPHA_* names in .reloc

Clean, idiomatic (`ELF_RELOC` X-macro over `ELFRelocs/Alpha.def`, the same pattern other
targets use), and the message correctly explains why it is needed
(`R_ALPHA_LITUSE` is unreachable from any instruction operand, so this is the only way
to test it). Test is tight — `CHECK-NEXT` throughout, exact addends.

Only nit: `//`/`///` comment style in a `.s` file; use `#`/`##`.
