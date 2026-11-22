<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-22T15:00:40.372Z","dependsOn":[]} -->
# Run primary-movement check before post-rebase-conflicted recovery

## Context

During `/external-skill-creator` validate-mode review of `finalize-changes`,
finding VS001 (Critical) identified a pre-existing recovery gap in
`.agents/skills/finalize-changes/references/worker.md` that was outside the
active change's implementation boundary:

- `.agents/skills/finalize-changes/references/worker.md:268-279`
  (`Postconfirmation rebase.conflicted`) specifies that after landing aborts
  its internal rebase, restores the approved single-parent candidate commit,
  and releases its lease, a fresh recovery worker re-resolves tips, runs the
  linear rebase and hunk resolution, and returns resolved regions before
  candidate preparation — without first running
  `Invoke-FinalizePrimaryMovementCheck.ps1` against the restored candidate and
  live primary.
- `.agents/skills/finalize-changes/references/worker.md:285-293`
  (`Preconfirmation reconciliation conflict`) defines the deterministic
  resolution for a conflict on a path listed in `changes.exportOutputPaths` of
  the triggering primary-movement result (take the primary tip's bytes, except
  keep the session's deletion on modify/delete, and include each game in
  `rebuildProjects` in the meaning-change handoff's `Build required`).
- Because the post-`rebase.conflicted` restart runs no primary-movement check,
  `.agents/skills/finalize-changes/references/worker.md:293-296` explicitly
  aborts every binary conflict of a reconciliation with no triggering result,
  naming the restart after `rebase.conflicted`.

Yet `.agents/skills/finalize-changes/scripts/Invoke-FinalizePrimaryMovementCheck.ps1:326-365`
is read-only and accepts `CandidateCommit`, `CandidateTree`, and
`CandidateParent` — all directly inspectable on the restored single-parent
approved candidate commit (`HEAD`, `HEAD^{tree}`, `HEAD^`) — and populates both
`changes.exportOutputPaths` (`:303`) and `rebuildProjects` (`:305`) even when
returning `blocked` / `primary.path-overlap` (`:363-365`). Meanwhile,
`.agents/skills/finalize-changes/SKILL.md:173-177` already routes
`-RequireExportOutputsRewritten` on later preparation briefs including after
`rebase.conflicted`. Consequently, a post-confirmation `rebase.conflicted`
caused by a tracked export-output binary conflict unnecessarily halts as a
blocker instead of taking the defined export-output resolution and rebuild
path.

## Design

Recommended approach: update `.agents/skills/finalize-changes/references/worker.md`
`## Recovery` so the fresh worker dispatched after proven post-confirmation
`rebase.conflicted` runs `Invoke-FinalizePrimaryMovementCheck.ps1` against the
restored single-parent candidate and live primary before rebasing, then feeds
that result into the existing conflict-resolution and rebuild rules. Rationale:
reusing `Invoke-FinalizePrimaryMovementCheck.ps1` without changing any script
supplies `changes.exportOutputPaths` and `rebuildProjects` from the single
authoritative classifier (`scripts.md#primary-movement-check`) and eliminates
the special-case abort for `rebase.conflicted`.

1. In `.agents/skills/finalize-changes/references/worker.md:268-279`
   (`Postconfirmation rebase.conflicted`): instruct the fresh recovery worker,
   after re-resolving tips and confirming the restored single-parent candidate
   commit, tree, and sole parent, to run
   `Invoke-FinalizePrimaryMovementCheck.ps1` (`scripts.md#invocation`) before
   starting the linear rebase:
   - On `primary.not-descendant`: stop and return the result as a blocker per
     `scripts.md:123` before any rewritten-history repair via
     `Repair-SessionForkPoint.ps1` (`worker.md:348-355`).
   - On `primary.path-overlap` (or `primary.disjoint-needs-review` / `pass` if
     live primary moved again): carry that result's `changes.exportOutputPaths`
     and `rebuildProjects` into the linear rebase and conflict resolution below
     (including passing non-empty `rebuildProjects` client/server builds on the
     meaning-change handoff even if the rebase merges cleanly).
   - On any unexpected `error` or other `blocked` movement-check result: return
     it as a blocker rather than rebasing without a valid movement result.
2. In `.agents/skills/finalize-changes/references/worker.md:280-298`
   (`Preconfirmation reconciliation conflict`): remove the obsolete
   `such as the restart after rebase.conflicted` exception at `:295-296`,
   keeping the rule that any non-export binary conflict (island textures
   included) or any binary conflict without a movement result listing the path
   in `changes.exportOutputPaths` aborts the rebase to return a blocker.

Change Workflow tier: Tier 2 — trigger: scoped behavior of the `/finalize-changes`
Change Workflow recovery procedure. No C++ source, PowerShell script,
determinism/CRC, wire, serialization, `.pack`/`kiVersion`, replay, affinity,
threading, allocation, shader, build, live `/agent-harness`, or shared-tool
bootstrap/build coordination exposure.

## Critical files

- `.agents/skills/finalize-changes/references/worker.md` — `## Recovery`
  (`:268-298`).
- `.agents/skills/finalize-changes/scripts/Invoke-FinalizePrimaryMovementCheck.ps1`
  — read-only reference (`:303-305`, `:326-365`; no edits).
- `.agents/skills/finalize-changes/references/scripts.md` — read-only reference
  (`:80-125`; no edits unless a link anchor needs updating).

## In scope

- `.agents/skills/finalize-changes/references/worker.md` (`## Recovery`,
  `:268-298`): run `Invoke-FinalizePrimaryMovementCheck.ps1` against the
  restored candidate and live primary before rebasing after post-confirmation
  `rebase.conflicted`, apply its `changes.exportOutputPaths` and
  `rebuildProjects` during conflict resolution, stop and return a blocker on
  `primary.not-descendant` per `scripts.md:123` before any rewritten-history
  repair (`worker.md:348-355`), return unexpected error/block results as
  blockers, and remove the stale `rebase.conflicted` no-result abort clause
  while preserving the abort on non-export binary conflicts.

## Out of scope

- Any PowerShell script edits under `.agents/skills/finalize-changes/scripts/`
  or `.agents/scripts/`.
- Changes to `.agents/skills/finalize-changes/SKILL.md` confirmation,
  standing-confirmation, or `-RequireExportOutputsRewritten` policy.
- Changes to `Invoke-FinalizePrimaryMovementCheck.ps1` schema or output fields.
- Changing non-export binary conflict handling (island textures and other
  non-export binaries must still abort the rebase and return a blocker).

## Acceptance criteria

1. `.agents/skills/finalize-changes/references/worker.md` `## Recovery`
   explicitly runs `Invoke-FinalizePrimaryMovementCheck.ps1` against the
   restored single-parent candidate and live primary on post-confirmation
   `rebase.conflicted` recovery, uses `changes.exportOutputPaths` to resolve
   tracked export-output binary conflicts, carries `rebuildProjects` into the
   meaning-change handoff's `Build required`, stops and returns a blocker on
   `primary.not-descendant` per `scripts.md:123` before any rewritten-history
   repair (`worker.md:348-355`), and still aborts non-export binary conflicts
   and unexpected movement-check failures.
2. `/external-skill-creator` validate mode on `.agents/skills/finalize-changes`
   and `/progressive-disclosure-review` pass with zero unresolved findings.
3. `Invoke-StaticChecks.ps1` exits `0` with `validate-skill`, `markdown-links`,
   `file-hygiene`, `line-length`, `model-names`, and `instruction-size` passing.

## Notes

- Keep the post-`rebase.conflicted` abort/restoration/lease-release proof
  unchanged (`worker.md:272-279`); the fresh recovery worker inspects the
  restored single-parent candidate only after those proofs hold.
- Check whether `worker.md` step 5's `rebuildProjects` handling and `SKILL.md`
  `### Landing confirmation` (`:173-177`, `-RequireExportOutputsRewritten`)
  already consume the meaning-change handoff without further edits; only update
  them if a direct contradiction is proven during implementation.
