# Architecture Review Worker

The invoking context's review steps, plus the lens checklists and finding
contracts the dispatched reviewers run. Triggers, inputs, and the handoff form
live in [`../SKILL.md`](../SKILL.md).

## Contents

- [Steps](#steps)
  - [Lens A — Dependencies and Layering](#lens-a--dependencies-and-layering)
  - [Lens B — Simulation and Threading](#lens-b--simulation-and-threading)
  - [Lens C — Client/Server and Data Shape](#lens-c--clientserver-and-data-shape)
  - [Lens D — Cohesion and Generation Residue](#lens-d--cohesion-and-generation-residue)
  - [Lens E — ThirdParty Replacement](#lens-e--thirdparty-replacement)
- [Rules](#rules)

## Steps

1. Resolve the scope before dispatch:

   - Exact file: review only that file. Follow dependencies and callers as
     evidence, but report a finding only when the defect is rooted in the target
     file or its contract.
   - Directory, non-recursive (default): enumerate only files directly in that
     directory. Do not silently include descendants.
   - Directory, recursive: include descendants only when the caller explicitly
     requests recursion.

   Done when the scope mode and its file set are resolved.
2. Build the scoped-file manifest, adding `-Recurse` only for an explicitly
   recursive scope and no extension filter, so every file type in scope,
   including shaders, is listed:

   ```powershell
   pwsh -NoProfile -File .agents/scripts/Get-AnalysisManifest.ps1 -Path <scope>
   ```

   Done when the manifest is in hand.
3. Take every in-scope file's repository-relative path and its ordered
   root-to-file `AGENTS.md` authority chain from that result; never reconstruct
   that enumeration or authority walk inline. Done when both come from the
   manifest.
4. On blocked (exit 2), narrow the scope and rerun; on error (exit 1), report
   the blocker. Done when the manifest is in hand or the blocker is reported.
5. Read the reported authority documents before dispatch. Done when every
   reported authority document has been read.
6. Preload every reviewer with those authority paths and the scoped-file
   manifest; require them to read the authorities before source. Done when every
   reviewer prompt carries them.
7. Inspect available depth and concurrency. Done when the currently free slots
   are known.
8. Include in every prompt:

   - the review scope and its exact scope mode;
   - the scoped-file manifest and authority paths;
   - the lens checklist for that lens;
   - the finding schema below;
   - external-claim routing;
   - the findings-only boundary.

   Done when each prompt carries every one of those.
9. Require every reviewer to report each finding in this exact shape. Lenses A,
   B, C, and E omit the Lens D evidence field entirely; Lens D alone includes it
   after `Evidence`.

   ```markdown
   - [severity/category] `path:line` — `symbol or contract`
     - Evidence: repository-observed fact; controlling authority citation when applicable
     - Lens D evidence: Interface surface: <caller/verification knowledge>; Deletion: <where complexity goes>; Seam: <actual variants or controlling invariant>; Locality/leverage: <concrete payoff>
     - Impact: concrete architectural or invariant consequence
     - Correction: smallest structural correction or investigation needed
     - Confidence: HIGH | MEDIUM | LOW
   ```

   Done when every returned finding matches that shape.
10. Require that reviewers not establish non-obvious external API,
    specification, license, maintenance, or ThirdParty behavior from memory.
    They return an External Claim Verification Request containing the exact
    proposition, dependent finding, applicable repository version/configuration,
    why it matters, and an official candidate source when known. Done when every
    such proposition arrives as a request instead of an asserted fact.
11. Dispatch one `reviewer` role per lens below, in rounds no larger than the
    currently free slots. Done when every lens has been dispatched.
12. Wait for the dispatched reviewers. Done when every lens reviewer has
    returned.
13. After reviewer rounds complete, route every such request through
    `verify-external-claims` before consolidation. Done when every request has a
    verdict.
14. Apply its verdict: retain `VERIFIED` evidence, remove or correct a `REFUTED`
    dependent finding, and move `UNRESOLVED` claims to residuals rather than
    presenting them as confirmed recommendations. Done when every request carries
    an applied verdict.
15. Deduplicate by root cause, preserve the strongest evidence, and
    cross-reference systemic findings without inflating their count. Done when no
    root cause appears twice.
16. Preserve every prioritized Lens-D recommendation's interface, deletion,
    seam, and locality/leverage evidence and payoff. Done when each of those
    recommendations carries them.
17. Choose one most impactful architectural improvement and name its modules,
    correction, and reason. Do not invent findings to populate a section. Done
    when the consolidated findings fill the handoff form.

### Lens A — Dependencies and Layering

- Map direct and transitive includes; identify hubs, large closures, cycles,
  missing direct includes, and unused includes. Treat macro, template, and
  transitive use cautiously.
- Find exported symbols with no production callers; distinguish test-only
  reachability.
- Check layer integrity. `Common` must not depend on `Engine` or `Projects`;
  `Projects` must not reach through documented engine boundaries. Engine reads of
  game globals or types allowed by applicable authorities are not violations.

### Lens B — Simulation and Threading

- Trace determinism risks: RNG ordering, parallel floating-point reorder,
  CRC-participating state, `SharedMembers()` parity, and phase-crossing reads or
  writes.
- Check dispatch ownership, shared state changes, worker lifetime, and
  Update/PostRender/Interpolate alignment against documented thread and frame
  rules.

### Lens C — Client/Server and Data Shape

- Check `BT_CLIENT`/`BT_SERVER` separation and mirrored build behavior.
- Check collection member registration, shared/client/member partitioning,
  interpolation versus PostRender placement, and cohesion.
- When scoped code is shader-facing, compare CPU and shader constants, layouts,
  and shared definitions.

### Lens D — Cohesion and Generation Residue

- Treat depth as a property of the interface: it is everything callers and
  verification must know. Prefer small interfaces that hide substantial
  complexity; investigate shallow indirection, god-managers, and excessive
  cross-manager knowledge.
- Apply the deletion test to a suspected shallow module by tracing where its
  complexity goes. Complexity redistributed into named callers is evidence the
  module earns its keep; complexity that disappears may indicate pass-through,
  but is investigation evidence only.
- Count adapters only when optional variation or swappability justifies the
  seam: one adapter is hypothetical, while two current concrete adapters normally
  demonstrate actual variation. Authority-required trust, platform,
  build-affinity, ThirdParty, producer/consumer, CPU/GPU, client/server, and
  other invariant contracts are exempt from adapter counting, but still require
  structural-impact and locality/leverage evidence.
- Assess reader load on two axes: how many layers a caller or verifier traverses
  to reach a value's origin, and how many mutable locations can change that
  value. For a suspected shallow seam, answer both from the repository — where
  does this value originate, and what can change it?
- Recommend collapsing a seam only when that tracing proves a net reduction in
  the sites a reader must know or a writer can change, and the reduction
  preserves determinism, serialization, trust boundaries, build affinity, CPU/GPU
  contracts, client/server behavior, and deliberate mirrors. Absent either half,
  report the tracing as investigation evidence and leave the seam in place.
- Do not recommend a new seam, adapter, test-only extraction, or testing
  infrastructure solely for testability.
- Find cosmetic or bypassed abstractions, abandoned sibling patterns,
  substantial cross-file duplication, dead modules, and producer/consumer seams
  with mismatched contracts.
- For each scoped function, check whether an existing shared helper in the same
  layer already owns that responsibility: search sibling and shared modules for
  helpers with overlapping field sets or terms. A re-implemented shared helper is
  duplication evidence even when clone detection reports no textual clone group.
- Require concrete structural impact plus demonstrated locality and leverage:
  concentrate change, knowledge, or verification and increase capability per
  interface knowledge. Do not infer a defect merely from stylistic difference or
  the history of AI generation.

### Lens E — ThirdParty Replacement

- Read `ThirdParty/AGENTS.md`, list existing ThirdParty packages, and avoid
  proposing an already-covered dependency unless extending it removes separate
  in-house code.
- Find cohesive, reusable in-house clusters with no engine-specific reason to
  exist. Skip engine pipelines, gameplay, collections, managers, and
  Vulkan/shader glue.
- Consider only replacements likely to remove more than 500 `bt-token-v1`;
  measure the inclusive candidate range with
  `pwsh -NoProfile -File .agents/scripts/Measure-Tokens.ps1 -Path <path> -StartLine <first> -EndLine <last>`.
- For each candidate, name the library, removable paths/ranges, integration and
  dependency risks, and the license proposition requiring verification. The local
  license allow list in `ThirdParty/AGENTS.md` remains controlling authority.

## Rules

- The main invoking context owns dispatch and synthesis. If this workflow is
  entered from a delegated context that repository policy forbids from
  delegating, return a main-context dispatch requirement; do not replace the five
  independent lenses with an inline approximation.
- Never ask a reviewer to delegate.
- Refill dispatch slots only as prior reviewers finish.
- Do not merge lenses merely to fit a host limit.
- Evidence may cross the boundary to prove a scoped finding; incidental defects
  outside it are residuals, not findings.
- Findings cite the controlling `AGENTS.md` path and section or line when a
  repository rule supplies the judgment. Checklists are heuristics, never
  authority.
- Lens-D tests are investigation heuristics only: they do not automatically
  establish findings or mandate abstraction or testing infrastructure without
  complete applicable evidence and concrete structural impact.
- Use precise symbols and evidence, not thematic summaries. Unused includes and
  dead-code candidates must retain confidence labels. Report no-finding
  conclusions explicitly for assigned lens checks.
