---
name: external-refactor-clean
description: >-
  Analyze a C++ file or directory for evidence-backed in-function refactoring
  opportunities and AI-generated residue. Use when the user explicitly runs
  /external-refactor-clean, or when /external-deep-analysis invokes its
  in-function phase. Route oversized files to /reduce-file and class, module,
  layer, or dependency-shape concerns to /external-architecture-review.
allowed-tools: [Read, Grep, Glob, Bash, Agent, PowerShell]
---

# Refactor Clean

## Purpose

Analyze local function mechanics and report findings only.

## When to use

- The user explicitly runs `/external-refactor-clean`.
- `/external-deep-analysis` invokes its in-function phase.

## Inputs

Require one target path and resolve it before inspection:

- Exact file: analyze only that `.h` or `.cpp`; never add siblings.
- Directory: analyze directly contained `.h` and `.cpp` files. Include
  descendants only when the caller explicitly requests recursion.

### Dispatch

After the manifest routes oversized files to `/reduce-file`, read file counts
above about 15 or aggregate size above about 80,000 bt-token-v1 as an indication
that a review split is useful, though not alone a requirement; smaller targets
may be inspected inline. The main invoking context may dispatch scoped rounds of
`reviewer` roles according to available capacity, and deduplicates their results.
Give each dispatched worker an explicit file manifest, authority map, inspection
rubric, and evidence/report contract, with each file belonging to one worker.

## Handoff

Fill every field of this schema for each finding, omitting empty
finding-category sections but never the manifest, coverage, handoff, or summary:

```markdown
## Refactor-Clean Analysis: <target>
Scope: <exact-file | directory-non-recursive | directory-recursive>

### Manifest and Authorities
- <path> — <tokens> — <analyzed | run /reduce-file <path>> — <authorities>

### Findings: <category>
- <path:line> — <function/local symbol> — Evidence: <code and reachable
  behavior>. Authority: <document and invariant>. Impact: <effect>. Smallest
  correction: <local boundary>. Verify: <decisive coverage>.

### Coverage
- <path> — inspected <functions/ranges>; checked <categories>; result
  <finding IDs | none | reduce-file handoff>

### Architecture handoff
- <path:line> — <class/module/dependency evidence> — run
  `/external-architecture-review <original target and scope>`
- none

### Summary
- Files: <enumerated/analyzed/oversized>
- Findings: <count by category and total>
- Observations not promoted: <threshold/search observations and reason>
- Residuals: <unreadable files, incomplete coverage, or external claims>
```

When invoked by `/external-deep-analysis`, retain its exact original target and
scope, consider Phase-1 investigation paths as evidence only, and return the
manifest, authority map, findings, coverage, residuals, and architecture handoff
without expanding the boundary.

## References

- [`references/worker.md`](references/worker.md) — the inspection steps and the
  evidence rules.
