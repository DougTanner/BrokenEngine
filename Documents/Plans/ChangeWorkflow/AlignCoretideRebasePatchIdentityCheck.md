<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-22T15:00:32.400Z","dependsOn":[]} -->
# Align Coretide clean-rebase patch identity check with engine landing

## Context

When primary advances before landing, both engine landing
(`.agents/skills/finalize-changes/scripts/Invoke-FinalizeLanding.ps1`) and
Coretide landing
(`.agents/skills/finalize-changes/scripts/Invoke-FinalizeCoretide.ps1`) permit
a clean internal rebase only when the rebased commit reproduces the exact
confirmed patch.

During `/external-skill-creator` validate-mode review of `finalize-changes`,
finding VS004 (Critical) identified a mismatch between the two scripts that was
pre-existing and outside that session's prose-only implementation boundary:

- `.agents/skills/finalize-changes/scripts/Invoke-FinalizeCoretide.ps1:52-78`
  (`Get-CoretidePatchId`) reads `git diff-tree -p --no-color --no-ext-diff
  --full-index --binary -r "$Commit^" $Commit` through text-decoded
  `Invoke-FinalizeGit` without `--no-renames`, re-encodes the string as UTF-8,
  and pipes it to `git patch-id --stable` (`:62`). In `-Land` (`:166-177`), it
  compares only that `--stable` hash (`:168`, `:173`) and runs no
  path/mode/status change guard.
- `.agents/skills/finalize-changes/scripts/Invoke-FinalizeLanding.ps1:544-621`
  (`Start-LandingGitProcess`, `Get-LandingPatchId`, `Get-LandingChangeGuard`,
  `Get-LandingPatchIdentity`) streams exact raw `diff-tree` bytes (including
  `--no-renames` at `:569`) directly into `git patch-id --verbatim` (`:579`,
  keeping whitespace significant while implying `--stable` so hunk line numbers
  stay ignored), normalizes an empty patch ID to `'(empty)'` (`:593`), and pairs
  it with `Get-LandingChangeGuard` (`:602-612`, `git diff --raw --no-renames
  -z`) to verify identical paths, modes, and statuses (`:675`).

Direct stdin probes confirmed that `git patch-id --stable` emits the same hash
for two whitespace-variant patches that `git patch-id --verbatim` distinguishes.
As a result, `Invoke-FinalizeCoretide.ps1 -Land` can fast-forward primary
Coretide with whitespace-altered, renamed, or mode-changed bytes that the engine
byte-identity contract rejects.

## Design

Recommended approach: extract the raw-byte patch identity and change-guard
helpers into `.agents/scripts/FinalizeWorkflowCommon.psm1` (already imported by
both `Invoke-FinalizeLanding.ps1:33-37` and `Invoke-FinalizeCoretide.ps1:29`)
and have both scripts call the shared helpers. Rationale: sharing one
implementation removes duplication and guarantees Coretide and engine
clean-rebase checks stay identical over time.

1. Move `Start-LandingGitProcess` (parameterized by working directory) and the
   implementations of `Get-LandingPatchId`, `Get-LandingChangeGuard`, and
   `Get-LandingPatchIdentity` (parameterized by `$Repository`, `$Base`, `$Tip`,
   and optional `$Pathspec`) into `.agents/scripts/FinalizeWorkflowCommon.psm1`
   as `Get-FinalizePatchId`, `Get-FinalizeChangeGuard`, and
   `Get-FinalizePatchIdentity` (preserving exact raw-stream transfer,
   `--no-renames`, `git patch-id --verbatim`, `'(empty)'` fallback, and
   `diff --raw --no-renames -z` path/mode/status comparison).
2. Update `.agents/skills/finalize-changes/scripts/Invoke-FinalizeLanding.ps1`
   (`:544-621`) to remove `Start-LandingGitProcess` and replace the bodies of
   `Get-LandingPatchId`, `Get-LandingChangeGuard`, and
   `Get-LandingPatchIdentity` with thin local delegates forwarding
   `$script:CurrentIdentity.Worktree` to the shared helpers, keeping all
   existing callers (`:388`, `:623-628`, `:666`, `:674`, `:835`, `:938`) and
   `Get-LandingNonHistoryPatchIdentity` unchanged.
3. Replace `Get-CoretidePatchId` in
   `.agents/skills/finalize-changes/scripts/Invoke-FinalizeCoretide.ps1:52-78`
   and update the `-Land` rebase comparison (`:166-177`) to compute
   `Get-FinalizePatchIdentity $coretide "$approved^" $approved` before the
   rebase and `Get-FinalizePatchIdentity $coretide $upstream 'HEAD'` after the
   rebase, blocking with `rebase.patch-not-identical` and resetting hard to
   `$approved` whenever either `.PatchId` or `.Changes` differs.

Change Workflow tier: Tier 2 — trigger: scoped behavior of one tooling
subsystem (`.agents/skills/finalize-changes/scripts/` and
`.agents/scripts/FinalizeWorkflowCommon.psm1`). No C++ source, determinism/CRC,
wire, serialization, `.pack`/`kiVersion`, replay, affinity, threading,
allocation, shader, build, live `/agent-harness`, or shared-tool
build/bootstrap coordination exposure.

## Critical files

- `.agents/skills/finalize-changes/scripts/Invoke-FinalizeCoretide.ps1` —
  `Get-CoretidePatchId` (`:52-78`) and `-Land` rebase check (`:166-177`).
- `.agents/skills/finalize-changes/scripts/Invoke-FinalizeLanding.ps1` —
  `Start-LandingGitProcess`, `Get-LandingPatchId`, `Get-LandingChangeGuard`,
  and `Get-LandingPatchIdentity` (`:544-621`), whose thin local delegates keep
  callers at `:388`, `:623-628`, `:666`, `:674`, `:835`, and `:938` unchanged.
- `.agents/scripts/FinalizeWorkflowCommon.psm1` — shared finalization module
  imported by both scripts.

## In scope

- `.agents/scripts/FinalizeWorkflowCommon.psm1`: add shared raw-byte
  `git patch-id --verbatim` and `git diff --raw --no-renames -z` patch-identity
  helpers parameterized by repository path.
- `.agents/skills/finalize-changes/scripts/Invoke-FinalizeCoretide.ps1`
  (`:52-78`, `:166-177`): replace `Get-CoretidePatchId` and compare both
  `PatchId` and `Changes` across the `-Land` rebase, resetting to `$approved` on
  mismatch with `rebase.patch-not-identical`.
- `.agents/skills/finalize-changes/scripts/Invoke-FinalizeLanding.ps1`
  (`:544-621`): remove `Start-LandingGitProcess` and replace
  `Get-LandingPatchId`, `Get-LandingChangeGuard`, and
  `Get-LandingPatchIdentity` with thin local delegates forwarding
  `$script:CurrentIdentity.Worktree` to the shared helpers so existing callers
  at `:388`, `:623-628`, `:666`, `:674`, `:835`, and `:938` remain valid with
  behavior preserved.

## Out of scope

- Any `Projects/Coretide/` repository files or Coretide game content.
- `Invoke-FinalizeCoretide.ps1` lock, receipt, branch, `-Prepare`, or
  plan-claim release behavior outside `:52-78` and `:166-177`.
- `Invoke-FinalizeLanding.ps1` recovery, history-overlay, compare-and-swap
  advance, or lease-cleanup logic outside patch-identity helper delegation.
- `.agents/skills/finalize-changes/SKILL.md` or `references/*.md` prose unless
  an exact script contract changes.

## Acceptance criteria

1. Comparing two commits differing only in whitespace through the shared
   `Get-FinalizePatchIdentity` helper (and therefore both
   `Invoke-FinalizeCoretide.ps1 -Land` and `Invoke-FinalizeLanding.ps1`)
   produces distinct `PatchId` values, whereas a clean rebase over an unrelated
   hunk that shifts only hunk line numbers produces matching `PatchId` and
   `Changes`.
2. A rebased Coretide commit whose file mode, path, or status differs from the
   approved commit (or collapses to an empty patch) fails the `.Changes` /
   `.PatchId` check, resets the clone hard to `$approved`, and exits `2` with
   `code: rebase.patch-not-identical`.
3. `Invoke-StaticChecks.ps1` exits `0` with all triggered checks passing.

## Notes

- Preserve raw byte streaming (`StandardOutput.BaseStream.CopyTo` into a
  `MemoryStream` and back to `StandardInput.BaseStream`) so binary diffs and
  non-UTF-8 or CRLF/LF bytes are never altered by text decoding before
  `git patch-id --verbatim`.
- Because all named files live under `.agents/`, this Plan belongs in
  `Documents/Plans/ChangeWorkflow/` rather than under `Projects/Coretide/`.
