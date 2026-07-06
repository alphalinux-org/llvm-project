# Review: Alpha backend series, commits 41–69

Range: `e0867294c3f7` … `8f7267359a64` (stack args, indirect calls, atomics, pre-BWX
byte/word synthesis, varargs, division millicode, jump tables, small data, CIX/FIX,
bswap, libcalls, half, cmov).

## Summary

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

## e0867294c3f7 — Pass arguments on the stack

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

## 9b1ed46c35ba — Lower indirect calls

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

## a5407c4342cc — Select atomic loads, stores and fences

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

## b9255a744df2 — Fence a sequentially consistent load on both sides

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

## 371b5fa192c9 — Lower atomic RMW with ldq_l/stq_c

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

## a3cd1ae11ccf — Lower atomic compare-and-swap

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

## 27dc8ed9d1db — Synthesize pre-BWX byte and word loads

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

## 4ea758cf6a00 — Synthesize pre-BWX byte and word stores

**Correctness** — the pattern-based sequence introduced here is the one `6328b37`
identifies as losing stores. It also introduces four instructions (`INSBL`, `INSWL`,
`MSKBL`, `MSKWL`) that all encode as bare opcode `0x12`.

**Structure** — squash with `6328b37`. Do not land a known data-loss miscompile as an
intermediate state; the surviving commit message can keep `6328b37`'s excellent
explanation of *why* the pseudo is needed.

**Tests** — the header comment ("Checking a particular order here asserts a scheduling
decision … and pins whichever order the compiler happened to produce on the day") is
good rationale but reads as prose essay; two sentences would do.

## 6328b37999b80 — Keep the pre-BWX byte/word store together

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

## 663dac63231e — safe-bwa feature

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

## a299923e57e1 — 32-bit atomic RMW

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

## bf53c3077575 — Select a frame index through lda

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

## 408a37e223b5 — Support variadic functions

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

## 23afada07d4b — Sub-word atomic RMW

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

## 76148385ea06 — Fold gprellow into constant-pool loads

**Correctness** — the fold is right and matches gcc.

- `LDLg`/`LDQg`/`LDTg`/`LDSg` are bare `AlphaInst` with only `Opcode`, so the
  `$Ra`/`$sym`/`$base` operands are not encoded.
- Lines exceed 80 columns:
  `                     "ldl $Ra, $sym($base)\t\t!gprellow", []> { let Opcode = 0x28; }`
- `AddedComplexity = 20` is unexplained; a one-line comment saying it must beat the
  plain `(load (AlphaGprelLo …))` + `LDAg` pairing would help.

**Tests** — `LDLg` (the `sextloadi32` pattern) is never exercised; there is no i32
constant-pool entry in the tests.

## a9f499b0f67c — Division and remainder via millicode

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

## 934b84f6f582 — Scaled add/subtract

**Correctness** — function codes `0x22`/`0x32`/`0x2b`/`0x3b` are correct for
S4ADDQ/S8ADDQ/S4SUBQ/S8SUBQ, and the `sub` operand order in the patterns
(`(sub (shl Ra, N), Rb)` → `sNsubq Ra, Rb, Rc`) is right.

Nothing to flag. Missing (fine as follow-up): the longword forms `s4addl`/`s8addl`.

## fa7763352c1b — Jump tables

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

## ad087baf804f — Small-data GP-relative addressing

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

## 8ab7cce666ee — Fold gprellow into small-data loads/stores

**Correctness** — opcodes `0x2c`/`0x2d`/`0x27`/`0x26` for `stl`/`stq`/`stt`/`sts` are
correct. The `GprelFold` multiclass is a clean refactor and the right shape.

- The `ST*g` defs are bare `AlphaInst` again.
- No fold for i8/i16 (BWX `ldbu`/`stb`), so small-data byte access keeps the extra
  `lda`. Fine as a follow-up, but the commit message's "a small-data global load or
  store is a two-instruction sequence" is broader than what is implemented.

**Tests** — only i64 load and store are checked. The f32/f64 store folds
(`STTg`/`STSg`) and the `truncstorei32` fold (`STLg`) are added but untested.

## 2a9ca1528b63 — CIX count instructions

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

## f54fd8aafd73 — FIX square root

**Correctness**

- `SQRTS` and `SQRTT` are both `AlphaInst` with only `let Opcode = 0x14` — identical
  encodings, and no `$Fa = 31`.
- The commit message says "otherwise it becomes a libcall", but runtime libcalls are
  not enabled until `be8f706` (six commits later), so on a non-FIX subtarget `fsqrt`
  at this point reports "no libcall available". Either reorder `be8f706` before this,
  or drop the claim.

**Tests** — only the `-mcpu=ev6` path. Add the non-FIX run line once `be8f706` is in
place (or after reordering).

## adb39a8bf310 — Expand bswap

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

## ebda925bb7f8 — Sub-word compare-and-swap

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

## a839b7e69896 — umulh and expanded signed multiply-high

**Correctness** — `UMULH` opcode `0x13` func `0x30` is correct. `MULHS`,
`SMUL_LOHI`, `UMUL_LOHI`, `ROTL`, `ROTR` all default to Legal in
`TargetLoweringBase::initActions`, so each `Expand` here is load-bearing. Good.

**Structure** — the rotate expansion is unrelated to multiply-high; either split it out
or retitle the commit. The message currently buries it in a subordinate clause.

**Tests** — `mulhs`'s `CHECK: umulh` / `CHECK: ret` does not verify the sign
correction, which is the whole content of the expansion. `divconst`'s
`CHECK-NOT: __divqu` is a good negative check.

## be8f706f9838 — Runtime libcalls and expanded FP operations

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

## f7232f0cd9a9 — Half precision via conversion libcalls

**Correctness** — the four `setOperationAction` calls plus the extload/truncstore
actions are the standard recipe; no issues.

**Commit message** — the only commit in the chunk without the "Verified under
qemu-alpha" tail, which is the right choice; apply it to the rest.

**Tests** — `half.ll` covers extend and truncate. Missing: a bare `half` load/store
with no conversion (memcpy-style), and `fpext half to double` (the f64 actions set
here are untested).

## 08ff50b405dc — BWX sextb/sextw

**Correctness**

- `SEXTB`/`SEXTW` use `OForm` with only `$Rb` in `ins`, so the `Ra` field is unbound
  and encodes as `$0` instead of the architecturally required `$31`. HEAD adds
  `Ra = 31` to the enclosing `let`; do it here.
- Function codes `0x00`/`0x01` under opcode `0x1c` are correct.

**Tests** — `sextbw.ll` covers both subtargets and the `bwx-mem.ll` update is right.
Good commit overall.

## 8f7267359a64 — Condition-specific conditional moves

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

## Cross-cutting recommendations

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
