<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-17T22:03:09.012Z","dependsOn":[]} -->
# Fix: New-CodexReviewPrompt.ps1 — a relative -RepositoryRoot is rejected although the skill implies the current repository root is valid

## Context

Run exactly as `.agents/skills/codex-review/SKILL.md:46` documents, from the
session worktree root, with the worktree input given as the current directory:

`pwsh -NoProfile -File .agents/skills/codex-review/scripts/New-CodexReviewPrompt.ps1 -RepositoryRoot '.' -Baseline <sha> -AssignedSkill <skill> -ScopeFile <path> -PromptPath <path>`

the script exited `2` with `code: prompt.repository-root-invalid` and message
`-RepositoryRoot must be an existing absolute directory: '.'`, producing no
prompt file. The guard is at
`.agents/skills/codex-review/scripts/New-CodexReviewPrompt.ps1:530-533`, where
the canonicalized root is additionally required to satisfy
`[IO.Path]::IsPathRooted($RepositoryRoot)` on the *supplied* string.

The skill text points the other way: `.agents/skills/codex-review/SKILL.md:30`
describes the input as "Worktree (default to current repository root)", and the
documented invocation is issued from the session worktree root, so a
current-directory value reads as valid. Nothing in the skill states that the
value must be absolute.

The workaround was one retry with the absolute worktree path, which succeeded;
the cost was one wasted invocation. Candidate shapes for the fix, which
root-causing decides: resolve a relative `-RepositoryRoot` against the current
directory and drop the `IsPathRooted` demand, or state in the skill's `## Inputs`
and invocation text that the parameter must be an absolute path.

The misbehaving surface is outside the `## In scope` boundary of the Plan this
session had claimed
(`Documents/Plans/Agents/VerifyChangesBuildReceiptDispatchInputs.md`), which
covers only the verify-changes dispatch-input documentation and names
`New-CodexReviewPrompt.ps1` explicitly in its `## Out of scope`. This is
`/next-plan` tooling friction, not an in-scope acceptance failure.

Verify every cited line number against the working tree before editing — the
numbers above are from this session and the files may have moved since.

Session provenance (machine-local; not reproducible after cleanup). The Client
through Worktree fields name the session that observed the friction — the session
`/next-plan-review` must reach — while the `Landing ref` line names a ref whose
tree actually contains this Plan:
- Client: claude
- Conversation session ID: b8b4eb54-6e35-498b-ab11-b4e67d65ecc1
- Worktree/branch UUID: 895e715f-5bb7-4307-95f5-f89d330e185c
- Session branch: claude/895e715f-5bb7-4307-95f5-f89d330e185c
- Worktree: .claude\worktrees\BrokenEngine\895e715f-5bb7-4307-95f5-f89d330e185c
- Landing ref: the session branch above, whose tip is that session's final commit
  and which survives exactly as long as the worktree recorded above. Fallback
  once the recorded ref is gone:
  `git log --diff-filter=A --format=%H -- <this plan path>`, but a periodic
  Plan-history squash can make it return an unrelated aggregate commit, so
  review its result only when the commit is attributable to one session alone
  (its diff limited to that session's files); never review an aggregate or
  multi-session squash commit.
- Run the review before /cleanup-worktrees removes the worktree above: Codex
  transcript discovery requires the producing worktree to remain registered, and
  Claude review requires the exact conversation session ID above.

## Design

In a new session, run `/next-plan-review <landing ref>` — the landing ref above —
supplying the recorded client and the recorded conversation session ID;
root-cause the friction from the proven transcript, then make the smallest fix
inside the `## In scope` boundary below. If root-causing shows the fix lies
outside that boundary, surface it for re-planning instead of expanding scope.

The outcome to deliver: an agent invoking the script exactly as the skill
documents it, from the session worktree root, either succeeds with a
current-directory worktree value or learns from the skill text before invoking
that an absolute path is required. Whichever of the two shapes is chosen, the
script guard and the skill text must agree.

## Critical files

- `.agents/skills/codex-review/scripts/New-CodexReviewPrompt.ps1` — the
  `-RepositoryRoot` parameter (`:9`) and its `IsPathRooted` guard and
  `prompt.repository-root-invalid` block (`:530-533`)
- `.agents/skills/codex-review/SKILL.md` — the `## Inputs` worktree bullet
  (`:30`) and the documented invocation line (`:46`)

## In scope

- Root-cause investigation via /next-plan-review, run with the recorded client,
  the landing ref named in `## Design`, and the recorded conversation session ID
- The smallest resulting fix, confined to the `-RepositoryRoot` resolution and
  its guard in `New-CodexReviewPrompt.ps1` (`:530-533`) and/or the `## Inputs`
  worktree bullet and invocation line in `.agents/skills/codex-review/SKILL.md`

## Out of scope

- The landed change the session produced
- Every other parameter, guard, and typed-artifact gate in the same script,
  including `Test-PromptScopeEvidence`
- The prompt template, the guardrail block, and the evidence collection
- `.agents/skills/codex-review/scripts/Test-CodexReviewPromptFixtures.ps1`, unless
  the chosen script-side fix leaves an existing fixture inaccurate
- Unrelated skills/scripts; any transcript path or transcript text in the repo

## Risk tier and invariants

Expected Tier 2 (scoped tool behavior); escalate if the fix reaches
build/bootstrap coordination. The guard must stay fail-closed on a value that is
not an existing directory, and the resolved root must remain the canonical
absolute path everywhere downstream, so no relative value can reach the child
inventory invocations. Never embed transcript paths or home paths.

## Acceptance criteria

- The recorded symptom no longer reproduces under the documented invocation:
  either the current-directory value is accepted and yields the same prompt as
  the absolute value, or the skill states the absolute-path requirement before
  the invocation is issued
- The script guard and the skill text agree on whether a relative value is valid
- `/validate-skill` passes for any changed `SKILL.md`; WorktreeCli `plan validate`
  exits `0` with `status: valid` and `code: ok`
