<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-15T22:50:46.495Z","dependsOn":[]} -->
# Fix: Find-AgentSessionTranscript.ps1 — `descendants` lists agent paths the parent session never delegated

## Context
During a `/next-plan-review` provenance pass the bundled finder was run from the
session worktree root as:

`pwsh -NoProfile -File .agents/skills/next-plan-review/scripts/Find-AgentSessionTranscript.ps1 -RepositoryRoot <worktree> -Commit codex/64b3ed0d-6e92-4e96-a927-5eb6d7c15cf3`

It returned `status: needs-selection` (exit 2, schema
`broken-engine-agent-session-transcript/v2`). The selected root candidate —
Codex session `01a005f6-0a1f-7842-9534-f44ed6f843c9`, `descendantCount` 32 —
carried a `descendants` array whose `agentPath` entries do not exist in that
parent session's own recorded delegation events. The finder reported
`/root/record_landing_lock_friction`, `/root/review_friction_plan`,
`/root/fix_friction_plan_finding`, `/root/rereview_friction_plan`, and
`/root/verify_friction_plan_landing`, none of which the parent ever spawned. The
parent's actual children are `/root/record_finalize_timeout_friction`,
`/root/review_finalize_timeout_plan`, `/root/scope_finalize_timeout_plan`,
`/root/rereview_finalize_timeout_plan`, `/root/rescope_finalize_timeout_plan`,
and `/root/verify_four_path_landing`; the friction-fix step went through a
follow-up task to the same authoring child rather than any separate "fix" child.
A delegated transcript reviewer that inventoried all 32 children directly from
the parent's own records established the mismatch.

Consequence: the finder's `descendants` metadata misattributes or renames
children, so any consumer trusting it builds a wrong routing inventory. No harm
occurred in this session only because
`.agents/skills/next-plan-review/SKILL.md` already forbids using `descendants`
as inventory authority, which forced the manual re-inventory above.

The false condition: the finder's `descendants` list accurately reflects the
parent session's recorded child agent paths.

The misbehaving script is outside the `## In scope` of the Plan claimed in the
observing session, so this is tooling friction and not an in-scope blocker. The
proven root cause is intentionally deferred to `/next-plan-review`.

Session provenance (machine-local; not reproducible after cleanup):
- Client: claude
- Conversation session ID: e8883f3d-63cf-4010-a100-28ff647bc073
- Worktree/branch UUID: f58d3bbc-46c0-4d8d-9b2d-e964715d9aea
- Session branch: claude/f58d3bbc-46c0-4d8d-9b2d-e964715d9aea
- Worktree: .claude\worktrees\BrokenEngine\f58d3bbc-46c0-4d8d-9b2d-e964715d9aea
- Landing ref: the session branch above, whose tip is the session's final
  commit and which survives exactly as long as the worktree recorded above.
  Fallback once that branch is gone:
  `git log --diff-filter=A --format=%H -- Documents/Plans/Agents/FinderDescendantAgentPathAttribution.md`,
  but a periodic Plan-history squash can make it return an unrelated aggregate
  commit, so review its result only when the commit is attributable to this
  session alone (its diff limited to this session's files); never review an
  aggregate or multi-session squash commit.
- Run the review before `/cleanup-worktrees` removes this worktree: Codex
  transcript discovery requires the producing worktree to remain registered,
  and Claude review requires the exact conversation session ID above.

## Design
In a new session, present the exact command `/next-plan-review <landing ref>` to
the user for them to run, supplying the recorded client and, on Claude, the
recorded conversation session ID. Root-cause how `descendants` entries and their
`agentPath` values are derived and associated with a root session, then make the
smallest fix inside the `## In scope` boundary below — correcting either the
association or the labeling so the reported children match the parent's own
records, or removing the field if it cannot be derived correctly. If
root-causing shows the fix lies outside that boundary, surface it for
re-planning instead of expanding scope.

## Critical files
- `.agents/skills/next-plan-review/scripts/Find-AgentSessionTranscript.ps1` —
  agent-path extraction around `:208`-`:242` and the root-to-descendant grouping
  and emission around `:376`-`:422`
- `.agents/skills/next-plan-review/scripts/Test-Find-AgentSessionTranscript.ps1`

## In scope
- Root-cause investigation via `/next-plan-review`, run with the recorded
  landing ref and client plus the recorded conversation session ID
- The smallest resulting fix, confined to the two files named above: the
  descendant `agentPath` derivation, the root-session association that groups
  descendants, and matching coverage in the test script
- The `descendants`/`descendantCount` contract wording in
  `.agents/skills/next-plan-review/SKILL.md` only if the accepted fix changes
  what those fields mean

## Out of scope
- The `/next-plan-review` review workflow itself, beyond any wording change the
  accepted fix forces
- The landed change the observing session produced
- Unrelated skills and scripts; any transcript path or transcript text in the
  repository

## Risk tier and invariants
Expected Tier 2 (scoped tool behavior); escalate if the fix reaches
build/bootstrap coordination or changes the
`broken-engine-agent-session-transcript/v2` schema shape consumers depend on.
Never embed transcript paths or home paths.

## Acceptance criteria
- For a root session, every reported `descendants[].agentPath` corresponds to a
  child the parent session actually recorded, and no recorded child is missing
  or renamed — or the field no longer claims to be that inventory
- `Test-Find-AgentSessionTranscript.ps1` covers the corrected association
- `/validate-skill` passes for any changed `SKILL.md`; `plan validate` exits 0

## Notes
This Plan is keyed to the concrete script-plus-symptom pair
(`.agents/skills/next-plan-review/scripts/Find-AgentSessionTranscript.ps1`
emitting `descendants[].agentPath` entries absent from the parent session's own
delegation records). A later observation of the same pair is a duplicate, not a
new residual.
