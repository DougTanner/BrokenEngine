<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-15T15:49:45.038Z","dependsOn":[]} -->
# Fix: compile runtime-data mode — Shared oracle passes while source/client format is incompatible

## Context

The documented compile workflow first ran
`pwsh -NoProfile -File .agents/skills/compile/scripts/Resolve-CompileContext.ps1`
and selected `dataBuildMode=Shared` with
`dataBuildModeDerivation=path-rules-only` for baseline
`cd07f0b95dcfb74db9164f6537a2500644e16ae6`. The resulting primary Shared Data
directory passed the mandated `New-DataOracleReceipt.ps1` producer and
`Test-DataOracleReceipt.ps1` verifier, each returning its versioned
`status:pass`, `code:ok` result. Both Debug client and server builds then
reported success with no diagnostics, and the prelaunch oracle verification
also passed.

The worktree source/client expected DataHeader version 426, while the current
primary Shared manifests were version 427 after primary regeneration. The
client exited before normal logging or the AgentHarness port because
`Engine/Source/File/PackChunks.cpp:167-174` reads each manifest's
`common::DataHeader` and rejects a version that differs from
`common::DataHeader::kiVersion`; the source-side version is defined at
`Common/DataFile.h:417-432`. A foreign rebuilt client using version-427 source
launched against the same Data directory. The source/data format mismatch was
therefore proven, while the exact-directory/mode/baseline oracle remained
correct. Multiple rebuild, oracle, and harness cycles were repeated before the
incompatibility was isolated.

The false required condition was that a successful Shared oracle plus a
successful compile was sufficient to prove that the selected runtime Data was
compatible with the source that produced the executable. The originating
compile acceptance requires the selected oracle to pass before and after each
game build and the runtime acceptance requires the built client/server to
launch; those checks did not detect this source/data version drift. The
claimed `Documents/Plans/Engine/UiStateHoistToGameBase.md` boundary contains
only UiState/GameBase C++ and ownership documentation, and runtime UI behavior
passed functionally, so the compile/data-mode workflow is out of scope rather
than an in-scope UI acceptance failure.

Session provenance (machine-local; not reproducible after cleanup):
- Client: codex
- Conversation session ID: none
- Worktree/branch UUID: f21d89f0-d1a2-48c5-ab51-8d31abef1593
- Session branch: codex/f21d89f0-d1a2-48c5-ab51-8d31abef1593
- Worktree: .codex\worktrees\BrokenEngine\f21d89f0-d1a2-48c5-ab51-8d31abef1593
- Landing ref: the session branch above, whose tip is the session's final
  commit and which survives exactly as long as the worktree recorded above.
  Fallback once that branch is gone:
  `git log --diff-filter=A --format=%H -- <this plan path>`, but a periodic
  Plan-history squash can make it return an unrelated aggregate commit, so
  review its result only when the commit is attributable to this session alone
  (its diff limited to this session's files); never review an aggregate or
  multi-session squash commit.
- Run the review before /cleanup-worktrees removes this worktree: Codex
  transcript discovery requires the producing worktree to remain registered,
  and Claude review requires the exact conversation session ID above.

## Design

In a new session, run `/next-plan-review <landing ref>` supplying the recorded
client and landing ref only. Root-cause the friction from the proven
transcript, then make the smallest fix inside the `## In scope` boundary below.
If root-causing shows the fix lies outside that boundary, surface it for
re-planning instead of expanding scope.

## Critical files

- `.agents/skills/compile/SKILL.md` — compile-context, runtime-data-mode,
  oracle, and game-build reporting contracts (`:51-69`, `:93-95`, `:144-155`).
- `.agents/skills/compile/references/runtime-data-mode.md` — Shared/Local
  selection, primary-refresh warning, receipt, and post-build verification
  rules (`:3-15`, `:34-50`, `:72-76`).
- `.agents/skills/compile/scripts/Resolve-CompileContext.ps1` — path-trigger
  derivation, baseline identity, selected data directories, and result
  envelope (`:31-37`, `:158-209`, `:211-258`).
- `.agents/skills/compile/scripts/New-DataOracleReceipt.ps1` — exact Data
  inventory and immutable receipt production (`:19-30`, `:164-197`).
- `.agents/skills/compile/scripts/Test-DataOracleReceipt.ps1` — receipt
  identity, content, and aggregate verification (`:20-31`, `:162-251`).

## In scope

- Root-cause investigation via `/next-plan-review` with the recorded
  provenance.
- The smallest resulting fix confined to the named compile skill sections,
  runtime-data-mode guidance, `Resolve-CompileContext.ps1`'s path-rule/result
  boundary, and the producer/verifier receipt identity checks. If the review
  proves that a caller outside these files is the source of the mismatch,
  surface it for re-planning instead of expanding scope.

## Out of scope

- The landed UiState/GameBase change and every file named by
  `Documents/Plans/Engine/UiStateHoistToGameBase.md`.
- `Common/DataFile.h`, `Engine/Source/File/PackChunks.cpp`, DataPacker source,
  primary data regeneration, and any runtime format, serialization, `.pack`,
  `.manifest`, or `kiVersion` behavior change; those are evidence of the
  mismatch, not an authorized fix boundary here.
- AgentHarness implementation or unrelated skills/scripts unless
  `/next-plan-review` proves the direct caller contract is the source and
  returns a re-planning pivot; no transcript path or transcript text in the
  repository.

## Risk tier and invariants

Expected Tier 2 (scoped compile/runtime-data tooling); escalate if the smallest
fix reaches build/bootstrap coordination, shared primary-data generation, or
runtime data-format/serialization code. Preserve one authoritative Data
directory per build, agreement among `DataBuildMode`,
`GameDataDirectory`, and `GeneratedDataIncludeRoot`, exact mode/baseline/path
identity in every oracle receipt, fail-closed behavior for changed or
unverified Data, and the prohibition on falling back from Local to Shared.
The tooling fix must not change `.pack`/`.manifest` bytes or add compatibility
code, and it must not silently accept a source/data `kiVersion` mismatch.
Never embed transcript paths or home paths.

## Coordination

- `Documents/Plans/Agents/CompileAbsoluteTargetPathResolution.md` records the
  separate pre-MSBuild absolute-target rejection. It shares the compile
  skill's invocation/workflow text but not the runtime-data boundary; the two
  Plans are order-independent, and whichever lands second must re-read the
  shared sections while preserving both target invocation and data-mode/oracle
  contracts.
- `Documents/Plans/Agents/WorktreeCliBuildTargetNormalizedIdentity.md` records
  a different successful-build target-identity mismatch. It can touch the
  compile skill's structured-result guidance, so any later edit must preserve
  this Plan's data-mode and receipt rules and must not treat a target-identity
  result as proof of runtime-data compatibility.

## Acceptance criteria

- For the recorded source/data drift, the documented resolve/build/oracle
  workflow no longer allows a successful Shared receipt and successful Debug
  build to proceed to a client startup `DataHeader` version mismatch; it either
  detects the incompatibility before launch or selects/requests a compatible
  data identity within the reviewed boundary.
- A genuinely compatible Shared source/data pair still passes the ordinary
  client and server build workflow without an unnecessary Local-generation
  requirement, and Local never falls back to Shared.
- A successful game build hands an oracle receipt whose exact path, mode,
  baseline, 23-entry inventory, and aggregate digest match the directory the
  executable consumes; pre/post verification remains fail-closed when that
  directory changes.
- The client reaches normal startup logging and the AgentHarness port after
  the recorded compatible build path, without requiring repeated replacement
  builds or oracle/harness cycles.
- `/next-plan-review` records the proven root cause and smallest fix within the
  named compile/runtime-data tooling boundary; if the root cause is outside
  it, the issue is surfaced for re-planning instead of expanding scope.
- `/validate-skill` passes if `.agents/skills/compile/SKILL.md` changes, and
  WorktreeCli `plan validate` exits `0` with `status:valid` and `code:ok`.

## Notes

This Plan is keyed to the concrete compile-context/runtime-data symptom:
`Resolve-CompileContext.ps1` selected Shared by path rules, the exact primary
Data directory passed both oracle receipts, and successful Debug builds still
exited before logging because source expected DataHeader version 426 while
manifests held version 427. A later observation of this same skill/script plus
observed symptom pair is a duplicate, not a new residual. The root cause is
intentionally deferred to `/next-plan-review`; this body records the command,
version evidence, workaround/repetition, and provenance without embedding
transcript material.
