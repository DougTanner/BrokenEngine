# Refactor Clean Worker

The inspection steps and the evidence rules. Triggers, inputs, and the report
schema live in [`../SKILL.md`](../SKILL.md).

## Steps

1. Build the manifest, adding `-Recurse` only when the caller explicitly
   requests recursion:

   ```powershell
   pwsh -NoProfile -File .agents/scripts/Get-AnalysisManifest.ps1 -Path <target> -Extension .h,.cpp
   ```

   Done when the manifest covers every in-scope file.
2. On blocked (exit 2), narrow the scope and rerun; on error (exit 1), report
   the blocker. Done when the manifest succeeded or the blocker was reported.
3. Take every in-scope file's repository-relative path, ordered root-to-file
   `AGENTS.md` authority chain, `bt-token-v1` size, and `reduceFileCandidate`
   flag from that result; never reconstruct that enumeration, authority walk, or
   per-file measurement inline. Done when those fields come from the manifest.
4. Read every reported authority document and `Documents/C++StyleGuide.txt`
   before inspection, and record the authority map in the report. Done when both
   are read and the map is recorded.
5. Measure a function directly with its inclusive range; the `bt-token-v1` value
   is normalized UTF-8 bytes divided by four and rounded up:

   ```powershell
   pwsh -NoProfile -File .agents/scripts/Measure-Tokens.ps1 -Path <path> -StartLine <first> -EndLine <last>
   ```

   Done when any function size the report cites was measured with that command.
6. Route every file the manifest flags as a `reduceFileCandidate` — headers over
   5,000 bt-token-v1 and implementations over 10,000 — directly to
   `run /reduce-file <path>`; do not inspect their functions here. Done when
   every flagged file is routed and none of its functions were inspected.
7. Trace functions, their local control flow, and reachable call sites needed to
   prove a candidate. Inspect for:

   - decomposable oversized functions, excessive control-flow nesting,
     unreachable statements, unused locals or parameters, and repeated local
     logic;
   - heap work in a demonstrated tracked hot path, missing suppression around
     unavoidable heap work, or allocation-dependent logging;
   - groups of local booleans better expressed as `common::Flags`, unaligned
     DirectXMath storage, vector operators instead of named DirectXMath
     functions, and guards wider than the locally differing statements;
   - needless defensive validation between trusted internal callers, useless
     assertions, swallowed failures, ambiguous default returns, reachable
     empty/single/zero boundary gaps, and acquire/release paths not protected by
     RAII;
   - dense narration comments and intra-file naming or TODO drift as local
     residue; hand actual formatting enforcement to `/code-style-review`.

   Done when every inspected function has been checked against each category.
8. Require concrete reachability or data-flow evidence. Parameter count,
   nesting depth, function size, a boolean count, or a search match alone is an
   observation. Done when every finding cites that evidence.
9. Recommend only the smallest local correction that preserves behavior and
   repository invariants. Done when each finding names that correction.
10. Keep class design, public signature redesign, ownership between types,
    cross-file duplication, include/dependency shape, cohesion, and layer
    placement out of findings. Done when none of that evidence appears as a
    finding.
11. List such evidence under `Architecture handoff` for
    `/external-architecture-review` without designing the fix. Done when every
    such piece of evidence sits in that handoff.
12. Apply repository rules only where their authorities cover the file; the
    repository-wide C++ conventions are in
    [`../../../references/cpp-conventions.md`](../../../references/cpp-conventions.md):

    - `Common/AGENTS.md` and `Engine/Source/Memory/AGENTS.md` govern workbuffer
      use, tracked-loop allocation, suppression, and the required `// Heap:`
      rationale.
    - `Common/AGENTS.md` and `Common/Log/AGENTS.md` govern allocation-free
      logging and workbuffer-backed formatting in tracked paths. Do not infer a
      violation from a format token without proving the call site's allocation
      boundary.
    - PCH-backed consumption headers belong in `Common/ExternalHeaders.h`; the
      PCH-less AgentTools instead centralize shared consumption headers in
      `Tools/ToolCommon/ToolCliCommon.h`. Respect documented
      implementation-unit exceptions. Header placement is not an in-function
      finding; route exposed dependency-shape evidence through the architecture
      handoff.
    - Treat a `*Base` reference as a candidate only when the file's authorities
      and an established game-derived surface require the derived type. Base
      definitions, deliberate base-layer code, and Engine reads of `game::gp*`
      globals are not violations; see `Engine/Source/AGENTS.md`.

    Done when every applied rule traces to an authority covering that file.

## Rules

- Do not edit source, perform security review, or turn style preferences and
  numeric thresholds into findings without behavioral or structural evidence.
  The `reduceFileCandidate` thresholds are routing observations, not proof of a
  defect.
- Heuristics in the steps above locate candidates; findings cite the controlling
  authority.
- A worker never delegates or starts another round; the dispatch guidance in
  [`../SKILL.md`](../SKILL.md) `### Dispatch` belongs to the main invoking
  context.
