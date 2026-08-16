<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-16T22:24:41.313Z","dependsOn":[]} -->
# Fix: codex-review — `-UntrackedPath` rejects the gitignored `Temp/` paths session artifacts are told to live in

## Context

During a `/next-plan` run, the manager assembled a `/plan-audit` review prompt
with the documented command

```
pwsh -NoProfile -File .agents/skills/codex-review/scripts/New-CodexReviewPrompt.ps1 ... -UntrackedPath 'Temp/FinalizeShortShaAcceptance-plan.md,Temp/FinalizeShortShaAcceptance-audit-scope.md'
```

It exited `2` with
`{"status":"blocked","code":"prompt.untracked-path-unknown","message":"The inventory reports no untracked entry for: Temp/FinalizeShortShaAcceptance-audit-scope.md, Temp/FinalizeShortShaAcceptance-plan.md"}`.

`Temp` is gitignored (`.gitignore:15`), so files placed there never appear as
untracked entries in the inventory the script consults, and the named-path check
at `.agents/skills/codex-review/scripts/New-CodexReviewPrompt.ps1:568-573` then
blocks the dispatch. The root `AGENTS.md` directs workers to put session files
under `Temp/` ("create a file under `Temp/` only when the owning workflow
requires one", echoed in `.agents/references/subagent-reporting.md:64`), while
`.agents/skills/codex-review/SKILL.md:70-71` says only that "`-UntrackedPath`
names every untracked file the review needs, and does not combine with `-Head`",
with no statement that an ignored path cannot be named and no conventional
location for review-evidence files. The workaround was to copy the plan file to
the repository root, where it is untracked and not ignored, and point the scope
text at that copy; that copy then had to be maintained for four further review
dispatches in the same session. Cost: one wasted invocation of a review-dispatch
script plus an undocumented divergence between where session artifacts are told
to live and where review evidence must live.

The claimed Plan executed this session,
`Documents/Plans/Agents/FinalizeApprovalCommitShaFormat.md`, confines its
`## In scope` to `.agents/skills/finalize-changes/references/scripts.md` and its
`## Out of scope` explicitly names "Unrelated skills/scripts", so
`.agents/skills/codex-review/` is outside the active change and this friction is
not an in-scope blocker of it.

Session provenance (machine-local; not reproducible after cleanup):
- Client: claude
- Conversation session ID: 8d55f528-7305-40b7-97ff-4aa5f6bab880
- Worktree/branch UUID: 1f46c5fb-02d8-4d14-939e-df714b93503e
- Session branch: claude/1f46c5fb-02d8-4d14-939e-df714b93503e
- Worktree: .claude\worktrees\BrokenEngine\1f46c5fb-02d8-4d14-939e-df714b93503e
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

The fix direction is documentation, not behavior: the script's refusal is
deliberate — its comment at
`.agents/skills/codex-review/scripts/New-CodexReviewPrompt.ps1:568-569` states
that a named path the inventory never reported would leave the prompt silently
missing evidence the manager believes it carries, and an ignored path is exactly
that case. So state in `.agents/skills/codex-review/SKILL.md`, where
`-UntrackedPath` is documented, that every named path must be untracked *and not
gitignored*, that a file under `Temp/` therefore cannot be named because `Temp`
is ignored, and name the location review-evidence files must use instead. Do not
change the script's inventory handling, blocked codes, or messages, and do not
make it accept ignored paths.

Verify every cited line number against the working tree before editing — the
line numbers above are from this session and the files may have moved since.

## Critical files

- `.agents/skills/codex-review/SKILL.md` — the `-UntrackedPath` documentation
  (`:45` invocation line, `:70-71` parameter guidance) and the
  `prompt.untracked-path-unknown` blocked-code entry near `:80`; this file is
  the authorized fix boundary
- `.agents/skills/codex-review/scripts/New-CodexReviewPrompt.ps1` — read-only
  reference for the enforced behavior (`:495-500`, `:568-573`); not edited

## In scope

- Root-cause investigation via `/next-plan-review`, run with the recorded
  landing ref, client, and conversation session ID
- The smallest resulting documentation fix, confined to
  `.agents/skills/codex-review/SKILL.md`: the `-UntrackedPath` guidance and the
  `prompt.untracked-path-unknown` blocked-code text, stating the non-ignored
  requirement and the location review-evidence files must use

## Out of scope

- Any change to `New-CodexReviewPrompt.ps1` behavior, its inventory source, its
  blocked codes or messages, or acceptance of gitignored paths
- `.gitignore`, the root `AGENTS.md` `Temp/` direction, and
  `.agents/references/subagent-reporting.md`, unless the review proves the
  documentation fix cannot be stated without one of them
- The landed change the session produced
- Unrelated skills/scripts; any transcript path or transcript text in the repo

## Risk tier and invariants

Expected Tier 1 (documentation only); escalate to Tier 2 if the review's proven
fix changes script behavior, and to Tier 3 if it reaches build/bootstrap
coordination. The prompt's evidence-completeness guarantee stays intact: a named
path the inventory never reported must keep blocking the dispatch. Never embed
transcript paths or home paths.

## Acceptance criteria

- `.agents/skills/codex-review/SKILL.md` states that `-UntrackedPath` accepts
  only untracked, non-gitignored paths and names where review-evidence files
  belong, so the recorded symptom is no longer reachable from the documentation
  alone
- `New-CodexReviewPrompt.ps1` is unchanged: naming a `Temp/` path still exits
  `2` with `prompt.untracked-path-unknown` and its existing message
- `/validate-skill` passes for `codex-review`; `plan validate` exits `0` with
  `status: valid` and `code: ok`

## Notes

This Plan is keyed to the concrete (script/skill, symptom) pair: the documented
`-UntrackedPath` parameter of the codex-review prompt builder gives no
indication that a gitignored path cannot be named, so pointing it at the `Temp/`
location session artifacts are told to use costs a failed invocation with
`prompt.untracked-path-unknown` plus a maintained copy elsewhere. A later
observation of the same pair is a duplicate, not a new residual.
`Documents/Plans/Agents/CodexReviewMetricsTargetsCircularInput.md` (metrics
digest and targets file each require the other) and
`Documents/Plans/Agents/CodexReviewPollWaitPattern.md` (the documented
poll-until-complete wait) cover different symptoms of the same skill, and
`Documents/Plans/Agents/SessionChangeInventoryUnstagedBlob.md` covers a
different script, so none is a duplicate of this one. The proven root cause is
deferred to `/next-plan-review`.
