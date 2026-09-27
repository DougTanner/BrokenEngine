<!-- broken-engine-plan/v1 {"createdUtc":"2026-09-27T17:50:22.206Z","dependsOn":[]} -->
# Fix: Invoke-Jev.ps1 — transient HTTP 520 fails a Jev request without retry

## Context
Observed during the `/code-style-review` mechanic pass of a `/next-plan` run of
`Documents/Plans/Engine/SettingsFileValueRejection.md`. The documented run
`pwsh -NoProfile -File .agents/scripts/Test-StyleRuleJudgment.ps1` (invoked as
`.agents/skills/code-style-review/references/worker.md` step 6 documents)
returned `status: error`, `code: jev.partial` ("Some requests failed"), with
several blocks carrying an `HTTP 520` error. Because the result is usable only
when `status` is `ok`, the review recorded "Judgment: not run" and fell back to
hand-reading only.

The failing requests come from `.agents/scripts/Invoke-Jev.ps1`, which
`Test-StyleRuleJudgment.ps1:289` calls and whose nonzero exit
`Test-StyleRuleJudgment.ps1:359` reports as `jev.partial`. In
`.agents/scripts/Invoke-Jev.ps1:86-87` the per-request retry loop (up to
`MaximumAttempts = 4`, `Invoke-Jev.ps1:20`) retries only HTTP 429 and 529 and
breaks immediately on any other status, so a transient 520 fails its request on
the first attempt. `Invoke-Jev.ps1:100` then turns any single failed request
into `jev.partial` for the whole run. The same path also serves
`Test-CitationSupport.ps1` (`:182`, `:224`).

Session provenance (machine-local; not reproducible after cleanup). The Client
through Worktree fields name the session that observed the friction — the
session `/next-plan-review` must reach — while the `Landing ref` line names a
ref whose tree actually contains this Plan:
- Client: claude
- Conversation session ID: f5f2680a-7560-4fd9-9c60-fe7ca8de7169
- Worktree/branch UUID: b40155bb-1b37-4e63-ad84-1ff399c0282f
- Session branch: claude/b40155bb-1b37-4e63-ad84-1ff399c0282f
- Worktree: .claude\worktrees\BrokenEngine\b40155bb-1b37-4e63-ad84-1ff399c0282f
- Landing ref: claude/b40155bb-1b37-4e63-ad84-1ff399c0282f
  Fallback once the recorded ref is gone:
  `git log --follow --diff-filter=A --format=%H -- Documents/Plans/ChangeWorkflow/JevRetryTransientHttp520.md`,
  but a periodic Plan-history squash can make it return an unrelated aggregate
  commit, so review its result only when the commit is attributable to one
  session alone (its diff limited to that session's files); never review an
  aggregate or multi-session squash commit.
- Run the review before /cleanup-worktrees removes the worktree recorded above:
  Codex transcript discovery requires the producing worktree to remain
  registered, and Claude review requires the exact conversation session ID
  above. OpenCode transcript review remains unsupported regardless of worktree
  retention.

## Design
First root-cause the friction from the current tree and this Plan's `## Context`.
Only when the transcript is genuinely needed, in a new session run
`/next-plan-review <review ref>` in bounded friction mode — the landing ref —
supplying the recorded client and the recorded conversation session ID. Then
make the smallest fix inside the `## In scope` boundary below. If root-causing
shows the fix lies outside that boundary, surface it for re-planning instead of
expanding scope.

The author recommends widening the retry predicate at
`Invoke-Jev.ps1:86-87` to cover the transient gateway statuses that 520
belongs to, keeping the existing backoff and attempt cap, and updating the
comment on L86 to state which statuses are retried and why; the fix session
confirms which statuses the TypeSafe API or its edge documents as transient
before choosing the exact set.

## Critical files
- `.agents/scripts/Invoke-Jev.ps1`

## In scope
- Root-cause investigation as `## Design` states
- The smallest resulting fix, confined to the retry predicate and its comment in
  the per-request `catch` block of `.agents/scripts/Invoke-Jev.ps1`
  (currently L86-87)

## Out of scope
- The landed change the session produced
- `Test-StyleRuleJudgment.ps1`, `Test-CitationSupport.ps1`, and the
  `/code-style-review` fallback to hand-reading on a non-`ok` result
- Attempt count, backoff timing, throttle, timeout, and result schema of
  `Invoke-Jev.ps1`
- Unrelated skills/scripts; any transcript path or transcript text in the repo

## Risk tier and invariants
Expected Tier 2 (scoped tool behavior: one script's retry behavior); escalate
if the fix reaches build/bootstrap coordination. Never embed transcript paths or
home paths. The API key must still never be printed.

## Acceptance criteria
- A request that receives HTTP 520 is retried under the existing attempt cap
  instead of failing on its first attempt, so a transient 520 no longer turns a
  `Test-StyleRuleJudgment.ps1` run into `jev.partial`
- The static-checks runner, invoked as `.agents/references/change-workflow.md`
  `#### Step 5 — Run targeted pre-review checks` documents it, reports every
  row the change triggers passing
