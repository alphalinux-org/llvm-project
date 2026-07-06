# Review: commits 70..100 (MC layer, disassembler, asm parser, clang target/ABI, TLS, misc codegen)

Range reviewed: `git log --reverse 0bf3638..HEAD | sed -n '70,100p'`, i.e.
`5e5e91b7aafb` .. `f406ccccb47c`.

## Summary

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

## 5e5e91b7aafb [Alpha] Add instruction encoding formats (operate, FP, lda)

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

## 35377a6ad64a [Alpha] Encode memory, branch and jsr-format instructions

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

## 36370f848867 [Alpha] Add MC code emitter, asm backend and ELF object writer

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

## d00ec074d435 [Alpha] Emit relocations and expand ldgp for object output

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

## 68b1b88ea461 [Alpha] Add disassembler

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

## c0680590ad5b [Alpha] Add assembly parser

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

## 986dd46fcfe5 [Alpha] Assemble relocation specifiers, ldgp and jsr

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

## b8c244111a8c [Alpha] Disassemble jsr, jmp, ldq_u and stq_u

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

## 1d852e99ad17 [Alpha] Support integer inline assembly operands

**The commit adds code the message says it does not add.** The message ends:

> Floating-point ("f") and memory ("m") operand constraints need further
> register-class and memory-operand handling and are not yet supported.

yet the diff adds `AlphaAsmPrinter::PrintAsmMemoryOperand`, which exists only
to print `"m"` operands. Without `SelectInlineAsmMemoryOperand` (added in the
next commit) it is unreachable dead code. Move it to `88616a50bb3d`.

**No test for the `MO_Immediate` path** in `PrintAsmOperand` (an `"i"`
constraint), which is one of the two cases the function handles.

## 88616a50bb3d [Alpha] Support memory-operand inline assembly constraints

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

## d996db159df7 [Alpha] Support floating-point inline assembly operands

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

## ddb35ca4231e [clang] Do not use musttail in the bytecode interpreter on Alpha

No objections. The message is slightly redundant ("the same way it does not on
PowerPC for the same reason" followed by "as the other targets in this list
do"); one of the two suffices.

## 191e891a25c2 [clang][Alpha] Add target support

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

## af0eb12ffa9a [clang][Alpha] Add driver support for alpha-linux-gnu

Values are right (`elf64alpha`, `/lib/ld-linux.so.2`, `lib` not `lib64`).

**Bulleted commit message.** LLVM commit messages are prose; the five-bullet
list of function names reads like a changelog. Fold to prose, and drop the
closing "Test in linux-ld.c checks the emulation and dynamic linker." — the
diff shows that.

**Test coverage**: `CHECK-ALPHA` checks `-m` and `-dynamic-linker` only. There
is no test for `getMultiarchTriple` or the `getOSLibDir` = `lib` decision,
both of which this commit adds and both of which are silent-misbehaviour
material.

## ef83df82488e [clang][Alpha] Give Clang the {base, offset} va_list

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

## 149ee63f25c7 [Alpha] Make va_list.__offset an int, as the ABI requires

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

## 55f8ff30af5a [clang][Alpha] Pass aggregates by value in the ABI

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

## a35542803717 [Alpha] Support local-exec thread-local storage

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

## b04619ec1634 [Alpha] Support initial-exec thread-local storage

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

## 5db347c39bd4 [Alpha] Support general-dynamic thread-local storage

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

## 845761ff9033 [Alpha] Support local-dynamic thread-local storage

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

## d69fddb12236 [Alpha] Lower BR_CC to test-and-branch against zero

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

## 44625d2bc56a [Alpha] Add -mbuild-constants to materialize wide constants inline

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

## 5fbe34714ba2 [clang][Alpha] Wire the -mbuild-constants driver flag

Clean; the driver test covers on/off/default. Two nits:

- The two options are inserted under the `// SPARC feature flags` comment in
  `Options.td`, inside `let Flags = [TargetSpecific]`. Give them their own
  `// Alpha feature flags` heading above the SPARC block, matching the pattern
  of every other arch there.
- `mno_build_constants` has no `HelpText`, so `clang --help` shows only half
  the pair. Most negative flags in that file follow the same (bad) pattern, so
  this is minor.

## 88ea133580bc [Alpha] Tag direct calls with hint and lituse_jsr relocations

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

## 7982d786041e [Alpha] Promote i1 loads to byte loads

Correct and minimal. Two nits:

- The test runs only with `-mcpu=ev6` (BWX). The pre-BWX path
  (`ldq_u`/`extbl`) is the more fragile one and is not covered.
- `use_bool`'s only check is `CHECK: ldbu`, which the first function already
  established; the function adds no coverage as written.

## 7f87b1815dfb [Alpha] Implement branch analysis

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

## a715d62c5180 [Alpha] Refuse to analyze a block with three terminators

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

## 67b1a5df7e81 [Alpha] Add mov and clr assembler pseudo-instruction aliases

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

## 873e86711b9b [Alpha] Select sub-word atomic loads

Correct — ordering is handled by `shouldInsertFencesForAtomic` returning true
(`AlphaISelLowering.h:206`, added earlier in the series), so a bare `ldbu` here
is right for the relaxed case and the fences are inserted by AtomicExpand.

**Only the zero/any-extending fragments are covered.** There are no
`atomic_load_asext_8/16` patterns. Reachable only if a sign-extending atomic
sub-word load survives to ISel; worth a comment saying why it cannot.

**Test asymmetry**: `l8` checks `ldq_u` *and* `extbl` in the NOBWX run, `l16`
checks only `extwl`. Add the `ldq_u` check for symmetry.

## f406ccccb47c [Alpha] Select sub-word atomic stores

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

## Cross-cutting recommendations

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
