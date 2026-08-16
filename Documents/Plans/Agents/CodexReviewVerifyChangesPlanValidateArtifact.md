<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-16T20:34:07.381Z","dependsOn":[]} -->
# Fix: codex-review prompt gate — /verify-changes is blocked for a host-run plan-validate receipt no Required inputs condition names

## Context

At a landing gate whose reviewed diff touched `Documents/Plans/**`, the
documented dispatch
`pwsh -NoProfile -File .agents/skills/codex-review/scripts/New-CodexReviewPrompt.ps1 ... -AssignedSkill 'verify-changes' ...`
exited `2` with

`{"status":"blocked","code":"prompt.typed-artifacts-required","message":"/verify-changes needs evidence -ScopeFile does not carry: \"operation\":\"validate\"."}`

The gate is
`.agents/skills/codex-review/scripts/New-CodexReviewPrompt.ps1:439`, which adds
`"operation":"validate"` to the missing-artifact list whenever the change set
touches `Documents/Plans/` and the scope text does not already contain that
marker; `:445-446` turns any non-empty list into the blocked exit.

Two documented contracts say the reviewer produces that evidence itself, not the
manager:

- `.agents/skills/verify-changes/SKILL.md:111-117` (`## Executable Plan check`)
  assigns the check to the reviewer — "When the reviewed diff touches
  `Documents/Plans/**`, run `... WorktreeCli.exe plan validate --lint-only --repo
  <absolute Git common directory> --worktree <adopted checkout>` and require a
  valid result for the session worktree."
- `:119-121` states that `--lint-only` "takes no scheduler guard, creates no
  storage, and heals nothing ... and the `/codex-review` read-only sandbox can
  run it."

Neither skill names a host-run plan-validate receipt as a required input.
`.agents/skills/verify-changes/SKILL.md:17-30` (`## Required inputs`) lists only
one typed artifact — "on a changed `SKILL.md`, the complete `/validate-skill`
PASS handoff" — and `.agents/skills/codex-review/SKILL.md:61-64` defers to that
list, requiring for `verify-changes` "the reviewed change set's baseline and head
SHAs plus each typed artifact its `## Required inputs` conditions name". The
plan-validate marker the script enforces is not one of those named conditions.

Forced rework: the blocked invocation was thrown away, the manager ran
`WorktreeCli.exe plan validate --lint-only` host-side, pasted the receipt text
into the scope file, and re-invoked the prompt script — one blocked invocation
plus one undocumented workaround round at the landing gate, after user
confirmation timing already mattered.

The misbehaving surfaces are the codex-review prompt-assembly script and the
verify-changes/codex-review input contracts. They were outside the `## In scope`
boundary of the work this session landed, so this is `/next-plan` tooling
friction and not an in-scope acceptance failure of that change.

Verify every cited line number against the working tree before editing — the
numbers above are from this session and the files may have moved since.

Session provenance (machine-local; not reproducible after cleanup):
- Client: claude
- Conversation session ID: 3e9eb8fe-a459-4eb1-9b42-614e3db7bd4a
- Worktree/branch UUID: 2476210d-562c-4f3a-92ba-aa7f638c717c
- Session branch: claude/2476210d-562c-4f3a-92ba-aa7f638c717c
- Worktree: .claude\worktrees\BrokenEngine\2476210d-562c-4f3a-92ba-aa7f638c717c
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
client and the recorded conversation session ID; root-cause the friction from
the proven transcript, then make the smallest fix inside the `## In scope`
boundary below. If root-causing shows the fix lies outside that boundary,
surface it for re-planning instead of expanding scope.

The outcome to deliver: a manager dispatching `/verify-changes` for a diff that
touches `Documents/Plans/**` follows one consistent contract — either the
evidence is not required from the host, or it is documented as required — with
no undiscoverable extra step at the landing gate. Two candidate shapes are
visible from the symptom, and root-causing decides between them; they are
alternatives, not a set to implement together:

1. Drop the plan-validate condition from the prompt gate, on the ground that the
   read-only sandbox reviewer already runs `plan validate --lint-only` itself
   per `verify-changes/SKILL.md` `## Executable Plan check`, so the host receipt
   is duplicated evidence the gate should not demand.
2. Keep the gate and document the receipt: add the plan-validate receipt to the
   `## Required inputs` conditions of `.agents/skills/verify-changes/SKILL.md`
   as a typed artifact the reviewed diff triggers, so the codex-review sentence
   that defers to those conditions resolves to it, and reconcile that with the
   reviewer-run check so the two are not silently contradictory.

Whichever shape is chosen, the prompt script's fixture coverage in
`.agents/skills/codex-review/scripts/Test-CodexReviewPromptFixtures.ps1`
(`:447-472` asserts the blocked code and the marker text) must end up consistent
with the contract that survives.

## Critical files

- `.agents/skills/codex-review/scripts/New-CodexReviewPrompt.ps1` — the
  `verify-changes` typed-artifact gate (`:428-447`), specifically the
  `"operation":"validate"` condition at `:439`
- `.agents/skills/codex-review/SKILL.md` — the required-evidence sentence for a
  `verify-changes` dispatch (`:61-64`) and the `prompt.typed-artifacts-required`
  blocked-code description (`:90-92`)
- `.agents/skills/verify-changes/SKILL.md` — `## Required inputs` (`:17-30`) and
  `## Executable Plan check` (`:109-123`)
- `.agents/skills/codex-review/scripts/Test-CodexReviewPromptFixtures.ps1` —
  the fixtures asserting the current gate behavior (`:447-472`), edited only if
  the chosen shape changes that behavior

## In scope

- Root-cause investigation via `/next-plan-review`, run with the recorded
  landing ref, client, and conversation session ID
- The smallest resulting fix, confined to the four files named above: the
  `verify-changes` typed-artifact gate in `New-CodexReviewPrompt.ps1`, the
  codex-review required-evidence and blocked-code wording, the verify-changes
  `## Required inputs` and `## Executable Plan check` text, and the matching
  fixture assertions

## Out of scope

- The landed change the session produced
- The `SKILL.md`/`Validation: PASS` typed-artifact condition, the baseline and
  head SHA conditions, and every other assigned skill's gate in the same script
- `Tools/WorktreeCli/**` behavior, `plan validate` semantics, output shape, and
  exit codes
- Unrelated skills/scripts; any transcript path or transcript text in the repo

## Risk tier and invariants

Expected Tier 2 (scoped tool behavior); escalate if the fix reaches
build/bootstrap coordination. The landing-gate plan validation must remain the
read-only `--lint-only` form that takes no scheduler guard, creates no storage,
and heals nothing, so the read-only review sandbox can still run it; the gate
must stay fail-closed for whatever evidence the contract does require. Never
embed transcript paths or home paths.

## Acceptance criteria

- The recorded symptom no longer reproduces: a `/verify-changes` dispatch for a
  diff touching `Documents/Plans/**`, assembled by following the documented
  codex-review and verify-changes inputs alone, produces a prompt without a
  `prompt.typed-artifacts-required` block and without an undocumented host-run
  step
- The codex-review gate and the verify-changes `## Required inputs` and
  `## Executable Plan check` text agree on who produces the plan-validate
  evidence
- `Test-CodexReviewPromptFixtures.ps1` passes against the contract that survives
- `/validate-skill` passes for any changed `SKILL.md`; WorktreeCli
  `plan validate` exits `0` with `status: valid` and `code: ok`

## Coordination

`Documents/Plans/Agents/VerifyChangesPlanValidateInvocationDiscovery.md` is
keyed to a different symptom of the same gate — discovering the working
`plan validate --lint-only` invocation took three attempts — and it edits the
same two skill files (`codex-review/SKILL.md` required-evidence and blocked-code
wording, `verify-changes/SKILL.md` `## Executable Plan check`). The two are not
directional: either may land first, and whichever lands second re-reads those
sections as they then stand and keeps the other's text intact. That Plan owns
making the required invocation discoverable at the point of use; this Plan owns
whether the host receipt is required at all and where that requirement is
stated. If this Plan's review selects shape 1 (drop the gate condition), the
other Plan's remaining work may shrink to nothing — surface that for re-planning
rather than deleting it from within this Plan.

## Notes

This Plan is keyed to the (script/skills, symptom) pair: the
`New-CodexReviewPrompt.ps1` `verify-changes` gate blocks on a host-run
plan-validate receipt that neither `verify-changes` `## Required inputs` nor
`codex-review` names as a required typed artifact, while `verify-changes`
assigns that same check to the reviewer. A later observation of the same pair is
a duplicate, not a new residual.
`Documents/Plans/Agents/CodexReviewMetricsTargetsCircularInput.md` (the
`repo-code-review` metrics digest input) and
`Documents/Plans/Agents/NextPlanListGitIdentityMismatch.md` cover different
symptoms of the same script and are not duplicates. The proven root cause is
deferred to `/next-plan-review`.
