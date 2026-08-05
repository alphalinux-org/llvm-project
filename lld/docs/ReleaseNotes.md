<!-- If you want to modify sections/contents permanently, you should modify both
ReleaseNotes.md and ReleaseNotesTemplate.txt. -->

(lld-release-release-notes)=

# lld {{ release | default("") }} Release Notes

```{contents}
:local: true
```

::::{only} PreRelease

:::{warning}
These are in-progress notes for the upcoming LLVM {{ release | default("") }} release.
Release notes for previous releases can be found on
[the Download Page](https://releases.llvm.org/download.html).
:::
::::

## Introduction

This document contains the release notes for the lld linker, release {{ release | default("") }}.
Here we describe the status of lld, including major improvements
from the previous release. All lld releases may be downloaded
from the [LLVM releases web site](https://llvm.org/releases/).

## Non-comprehensive list of changes in this release

### ELF Improvements

* Added support for the Alpha (`EM_ALPHA`) target: GP-relative addressing and
  multi-GOT partitioning, and the four TLS models.
* `--relax` on Alpha rewrites a `jsr` through the GOT into a direct branch, and
  drops the GOT load along with it where the callee does not need its own
  address. As on the other targets that support it, `--relax` is on by default;
  pass `--no-relax` to keep every call as the compiler emitted it.

### Breaking changes

### COFF Improvements

### MinGW Improvements

### MachO Improvements

### WebAssembly Improvements

#### Fixes
