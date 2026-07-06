# Review: commits 1..11 (generic prerequisites)

Base: `0bf3638ddfe6e8bb3b79ebe8c2a918384a5df612`, branch `alpha-triple-0bf3638`.

## Summary — highest priority

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

## `6f4096c225c5` — [TailCallElim] Do not mark a call tail when it is handed the frame

### Correctness

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

### Commit message

Good — it explains why, cites the reproducer, and does not narrate the debugging. Two nits:

- "gcc.c-torture/execute/frame-address.c aborts on this on every target" — "on this on"
  is a typo/awkwardness.
- The em-dash-delimited aside "`-- it passes __builtin_frame_address(0) to a call whose
  comment says it exists to prevent exactly the tail call that was happening --`" is one of
  three `--` pairs in an 11-line message. Trim to one.

---

## `313d580e0349` — [TailCallElim] Make the frame-address test able to fail

### Structure — **squash into `6f4096c225c5`**

This is a fix to the immediately-preceding commit's test. Upstream this is not two commits;
it is one commit with a working test. The commit message here is a narration of the author
discovering their own mistake ("which is to say the only test for this change could not
detect the change being reverted"), which has no place in the permanent history once
squashed.

### Commit message

- `Fixes ALPHA-T01.` — a private tracker ID. Must be removed.
- "Rewriting the pass's output to mark all four calls tail and running the test over it
  passes" — this describes a manual verification the reader cannot reproduce.

### Test content

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

## `fae618baf9c5` — [TargetParser] Add DEC Alpha triple

### Correctness

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

### Tests

`llvm/unittests/TargetParser/TripleTest.cpp:89-97` adds one parse case. Missing: the
`get64BitArchVariant`/`get32BitArchVariant`/`getBigEndianArchVariant` round-trips (there are
dedicated `TEST`s for those in the same file that were not touched), and the data layout.

### Commit message

The body is a bulleted list flattened into prose — "the ArchType enum, name/prefix
accessors, both LLVM-name and user-triple parse tables, pointer width (64), endianness
(little), 32/64-bit and big/little-endian variant maps, default object format (ELF),
default exception handling (DWARF CFI), and an isAlpha() predicate" — which restates the
diff rather than explaining anything. The second and third paragraphs (why the data layout
is what it is, and that this is groundwork) are the useful part. Cut the first paragraph
to a sentence.

---

## `05090f24c5b7` — [BinaryFormat] Correct EM_ALPHA to the value real objects use

### Correctness — the headline concern

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

### Tests

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

### Structure

The `ELFObjectFile.h` hunks depend on `Triple::alpha` from `fae618baf9c5` — ordering is
correct.

---

## `2fb9f74d8aa4` — [BinaryFormat][Object] Add the Alpha ELF relocations

### Correctness

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

### Tests

- `llvm/test/tools/llvm-readobj/ELF/reloc-types-alpha.test:1-2`:

  > `## Test that llvm-readobj/llvm-readelf shows proper relocation type`
  > `## names and values for the alpha target.`

  Only `llvm-readobj` is run. Every sibling `reloc-types-*.test` runs both. Either add the
  `llvm-readelf` RUN line (with the corresponding `--check-prefix`) or fix the comment.

### Commit message

"Add the R_ALPHA_* set (0-41 ...)" — the set is not 0-41; nine numbers in that range are
deliberately absent. Say "0-41 with the deprecated and Compaq-only numbers omitted".

---

## `3caa5a0a3714` — [MC] Widen MCSymbolELF's st_other storage to five bits

### Correctness

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

### Tests

**None.** `setOther`/`getOther` round-tripping bits 3-7 is exactly the kind of thing
`llvm/unittests/MC/` exists for, and the commit message claims the round trip as its result.
Without a target that emits `st_other` bit 3 (which does not land until much later in the
series), this commit is completely uncovered. Add an `MCSymbolELF` unittest, or move this
commit adjacent to the Alpha MC commit that needs it and give it an `.s` test there.

---

## `860c777d8c46` — [MC] Skip AsmToken::Comment at statement start in AsmParser

### Correctness

`AsmToken::Comment` is produced by `AsmLexer.cpp:297`, and `AsmParser.cpp:916` already
loops over `AsmToken::Comment` in the pre-statement path, so skipping it in `parseStatement`
is consistent with existing handling.

- A reviewer will ask why the fix is not in `eatToEndOfStatement()` — the function that
  leaves the stray token. If skipping at statement start is the right layer, the commit
  message should say why (e.g. other raw-lexer paths can leave it too). Right now it just
  describes the symptom.

### Commit message / comments

- `AsmParser.cpp:1728-1730` — the three-line comment is a verbatim restatement of the
  commit message body. One of the two should be trimmed; the code comment can be one line
  ("eatToEndOfStatement() lexes raw and can leave a block comment as the current token").
- "so they do not confuse start-of-statement logic" — vague. Say what actually goes wrong:
  a comment-only line is taken as the start of a statement and produces a spurious
  `unexpected token at start of statement` error. The test comment gets this right; the
  commit message and the code comment do not.

### Tests

`llvm/test/MC/AsmParser/block-comment-after-error.s` is a genuine regression test and the
`CHECK-NOT` is correctly scoped after the `:9:10:` match. Two notes:

- The `## ... ##` preamble (lines 4-7) is four lines to explain a two-line test. Two lines
  would do.
- The test only covers a block comment left behind by an *error* path. If the raw lexer can
  strand a comment on non-error paths too, that case is untested — but if it cannot, the
  commit message's general claim ("Block comments can be left as the current token by
  eatToEndOfStatement()") should be narrowed to the error recovery path.

---

## `42f2bef50b85` — [MC] Increase asm-macro-max-nesting-depth default to 100

### Correctness / commit message — **the message and the code comment disagree**

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

### Tests

- `llvm/test/MC/AsmParser/macro-max-depth.s:2` — pinning the limit rather than relying on
  the default is the right call, and the added comment ("which is a tuning knob") is
  appropriate.
- Nothing tests the new default of 100. A RUN line with 30 nested macros and no explicit
  `-asm-macro-max-nesting-depth` would make the change itself testable; as it stands the
  behavior change this commit is *about* has no coverage.

---

## `61e17ecc3c70` — [clang] Diagnose the _BitInt suffix as an extension in GNU C modes

### Correctness

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

### Tests

Good coverage: `gnu` (with `-Wbit-int-extension`) proves the new diag fires,
`gnuquiet` (default flags) proves it is silent by default, and the `__wb`/`__uwb` rows prove
the invalid-suffix path is unaffected. This is the strongest test in the chunk.

- `clang/test/Lexer/bitint-constants-compat.c:4-7` — the added comment is the commit message
  pasted in, including the symmetric "`not as a use of a C23 feature, and not as pre-C23
  incompatibility`" construction. Delete it or reduce it to naming the two new RUN lines.

### Missing

No `clang/docs/ReleaseNotes.rst` entry. A new user-visible diagnostic (and a behavior change
under `-std=gnu17 -Werror`) needs one; upstream clang reviewers ask for this every time.

---

## `a847f0253aa4` — [clang] Silently ignore GCC's noclone attribute

### Correctness — **three contradictory rationales in one commit**

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

### Structure / naming

- `Attr.td:2426` — the def is named `GCCNoClone` and placed between `NoInline` and
  `NoOutline`. The other `IgnoredAttr`s (`Bounded`, `NvWeak`, `Win64`, the anonymous
  `Declspec<"property">` one) are named after the spelling without a vendor prefix. `NoClone`
  is free; the `GCC` prefix is redundant with `let Spellings = [GCC<"noclone">]`.
- `IgnoredAttr` sets no `Subjects`, so `__attribute__((noclone)) int x;` is silently accepted
  on a variable. That is standard `IgnoredAttr` behavior, so it is fine — but it means the
  test's function-only cases do not pin much.

### Tests

`clang/test/Sema/attr-noclone.c` — three declarations, all `expected-no-diagnostics`.

- Nothing tests that `noclone` with arguments (`__attribute__((noclone(1)))`) is diagnosed.
  GCC's `noclone` takes none, and `IgnoredAttr` still runs the arg-count check.
- The three cases (plain, alongside `noinline`, on a redeclaration) all exercise the same
  code path — "attribute is ignored". One would do; the redeclaration case is the only one
  that could conceivably differ.

### Missing

No `clang/docs/ReleaseNotes.rst` entry for a newly accepted GCC attribute.

---

## `dad7b844f932` — [clang] Suppress -Wuninitialized for unevaluated builtin args

### Correctness

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

### Tests

`clang/test/Sema/warn-uninit-unevaluated-builtin.c` is `// expected-no-diagnostics` with four
functions that must produce nothing.

- **The test passes vacuously** if `-Wuninitialized` regresses or the RUN line's flag is
  dropped. Add a negative control — one function that *does* pass an uninitialized variable
  to a normal call and *does* warn — so the test proves the warning machinery is live.
- The two builtins the commit actually changes behavior for (`__builtin_classify_type`,
  `__builtin_constant_p`) are covered; the two pre-existing ones are regression guards, which
  is fine. None of the three *newly affected* builtins listed above are tested.

### Commit message

- `"preventing false positives from dataflow analyses like -Wuninitialized"` and
  `"This extends correct treatment to ..."` — "This extends" / "This ensures" phrasing;
  state it directly ("`__builtin_classify_type` and `__builtin_constant_p` also carry
  `UnevaluatedArguments` and were not covered").
- The parenthetical `"(the variables' values are never read; only their types are inspected)"`
  is correct for `classify_type` but not for `constant_p`, which inspects constant-ness, not
  the type.

### Missing

No `clang/docs/ReleaseNotes.rst` entry for the `-Wuninitialized` behavior change.
