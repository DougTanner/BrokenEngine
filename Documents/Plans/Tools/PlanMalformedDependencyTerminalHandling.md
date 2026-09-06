<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-30T15:56:20.654Z","dependsOn":[]} -->
# Fail closed on malformed dependency children during terminal plan operations

## Context

The frozen audit finding `CAI/shard-0061/002` confirms a terminal-operation
gap in `Tools/WorktreeCli/PlanMetadata.cpp` and
`Tools/WorktreeCli/PlanScheduler.cpp`.  `ParsePlanBytes` returns false before a
complete dependency relation is available for malformed marker/JSON or bad
`dependsOn` data, while `BuildPlans` retains the tracked path in its plan map.
`RunTerminal` currently searches only the parsed `Plan::dependencies` vector
for the target, so it can skip the invalid child and reach the success path.

The current reproduction is a claimed target Plan plus a tracked child whose
byte-zero marker has a trailing-comma `dependsOn` array naming that target.
`WorktreeCli.exe plan complete --repo ... --worktree ... --owner ...
--session ...` and the corresponding authorized
`plan reject ... --user-authorized-rejection` currently exit 0 with the v2
success envelope, delete the target, and leave the child bytes unchanged.
This violates the existing final-operation rule that an invalid dependency
child is a state conflict, while unrelated invalid Plans must remain isolated.

## Design

Author's recommendation, reflecting the resolved user decision: make the
parser authoritative for a three-state dependency relation and add an
internal dependency-knownness state to `Plan`.  A complete, fully decoded
`dependsOn` array whose every element is a canonical path establishes either a
known direct relation or a known unrelated relation, even when other metadata
is invalid.  Duplicate and unsorted arrays remain relation-known.  A malformed
marker or JSON document, missing or non-array `dependsOn`, or any nonstring,
noncanonical, or partially decoded element is unknown.  JSON strings are
decoded before path normalization.

In `RunTerminal`, skip the target first.  For every other existing,
non-guidance tracked Plan, return the existing `schemaVersion: 2`,
`status: error`, `code: child-invalid`, exit-2 response before target checks,
temporary cleanup, staging, or any mutation when the relation is known direct
but invalid, or when the relation is unknown.  A known-unrelated invalid Plan
does not block.  Apply the same preflight to `complete` and authorized
`reject`; preserve valid child rewrites, atomic staging/rollback and cleanup,
target existence and untracked-target behavior, claims, selection isolation,
guidance inertness, tracked-but-deleted child nonblocking behavior, and
fail-closed handling of unreadable existing bytes.  Keep validation diagnostic
priority, marker/result schemas, and the current CLI surface unchanged: do not
parse raw tokens, globally block on invalid Plans, or add a new command.

## Critical files

- `Tools/WorktreeCli/PlanMetadata.h` — `Plan`'s internal dependency-knownness state.
- `Tools/WorktreeCli/PlanMetadata.cpp` — `ParsePlanBytes` and `ClassifyDirectoryGuidance` relation classification.
- `Tools/WorktreeCli/PlanScheduler.cpp` — `RunTerminal` preflight shared by `complete` and `reject`.
- `.agents/scripts/Test-WorktreeCliPlanScheduler.ps1` — terminal fixture scenarios and byte/mutation assertions.
- `Tools/WorktreeCli/AGENTS.md` — scheduler final-operation authority prose.

## In scope

- The internal dependency-knownness state on `Plan` and parser-authoritative
  direct/unrelated/unknown classification in `ParsePlanBytes`.
- Guidance and missing-file handling in `ClassifyDirectoryGuidance` required
  to keep guidance inert and tracked-but-deleted children nonblocking.
- The `RunTerminal` preflight for both terminal commands, including the exact
  child-invalid response and its no-mutation ordering.
- Official scheduler fixture scenarios covering complete and authorized reject,
  relation forms, diagnostics, controls, and atomic no-mutation behavior.
- The corresponding final-operation contract in `Tools/WorktreeCli/AGENTS.md`.

## Out of scope

- Plan marker or command-result schema changes, CLI additions, raw-token
  dependency parsing, or a global invalid-Plan block.
- Selection, dependency-cycle, claim, target-existence, untracked-target,
  atomic publish, rollback, or temporary-file behavior except for the required
  preflight coverage and preservation above.
- Other WorktreeCli commands, other tools, game/runtime code, compatibility
  formats, and unit tests.

## Risk tier and invariants

Expected future Change Workflow Tier 2: scoped WorktreeCli behavior and a
tighter check at the existing metadata trust boundary, with no format or trust
boundary change.

Preserve these invariants:

- A known direct invalid child or unknown non-guidance existing child prevents
  both terminal commands from deleting or rewriting anything.
- A known unrelated invalid Plan does not block the requested target; guidance
  remains inert and a tracked-but-deleted child remains nonblocking.
- Valid direct children are rewritten atomically, with existing rollback,
  cleanup, target, claim, and result behavior intact.
- Validation diagnostics and priority, selection isolation, marker/result
  schemas, and valid terminal behavior remain unchanged.

## Acceptance criteria

- With a trailing-comma direct child, both `plan complete` and authorized
  `plan reject` return `schemaVersion: 2`, `status: error`,
  `code: child-invalid`, and exit 2 before target checks, temporary cleanup,
  staging, or mutation; target, child, claim, and coordination bytes are
  unchanged.
- The fixture matrix proves that escaped canonical strings decode before
  normalization; duplicate and unsorted arrays remain relation-known; a
  complete canonical array still proves direct or unrelated when other
  metadata is invalid; and malformed marker/JSON, missing/non-array
  `dependsOn`, and nonstring/noncanonical/partial elements are unknown.
- Known-unrelated invalid Plans do not block either terminal command, while
  known-direct invalid and unknown non-guidance existing Plans do; valid direct
  children still rewrite and the target still deletes as before.
- Missing targets, guidance files, valid unrelated children,
  tracked-but-deleted children, unreadable existing bytes, target
  existence/untracked behavior, claims, and selection isolation retain their
  current outcomes.  Validation retains its existing diagnostics and priority,
  and marker/result schemas are unchanged.
- WorktreeCli Debug and Release builds pass through `/compile`, and the
  official fixture candidate invocation passes:
  `pwsh -NoProfile -File .agents/scripts/Test-WorktreeCliPlanScheduler.ps1
  -WorktreeCliExecutable <candidate>`.
- After `/update-claude-docs`, a fresh `/progressive-disclosure-review` passes
  for the changed WorktreeCli authority prose.

## Notes

Frozen provenance is `CAI/shard-0061/002`, audit commit
`76d303f0eeeb86c1ed241edc81634e60070ba5a5`, and
`Temp/CppAdversarialInvariantAudit/76d303f0eeeb86c1ed241edc81634e60070ba5a5/shard-0061.md`.
This Plan replaces the removed
`Documents/Investigations/Agents/PlanMalformedDependencyTerminalHandling.md`;
the conversion itself changes no WorktreeCli source or fixture behavior.
