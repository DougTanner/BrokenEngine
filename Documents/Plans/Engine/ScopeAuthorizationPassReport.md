<!-- broken-engine-plan/v1 {"createdUtc":"2026-09-05T20:23:03.792Z","dependsOn":[]} -->
# Fix: scope-authorization.md — it never says what a passing authorization pass reports, so the reviewer narrated the whole scope mapping

## Context
A `/coherence-review` reviewer dispatched in this session returned a 5,718-
character handoff on a change with no scope finding. Roughly 1,200 characters
of it were an inline per-region narration of the authorization mapping — each
changed region paired with the `## In scope` clause that authorized it — plus
sub-bullets for the minimality and KISS passes that had also found nothing. The
manager needed one fact from all of it: that the change was authorized.

`.agents/references/scope-authorization.md:12-26` describes the three passes
and then names only what a failure reports: `Scope: not supplied` when the
authorization source is missing, an `unauthorized` finding for an unmapped or
out-of-scope region, flagged items for the minimality and KISS passes, and
`Every finding cites the specific clause violated`. Nothing in the reference
states what a pass that finds nothing reports, so a reviewer that shows its
mapping work is not contradicting anything written. The reference is shared by
every Review and resolve correctness dispatch at Tier 2+, so the same narration
can arrive from any of them.

Session provenance (machine-local; not reproducible after cleanup). The Client
through Worktree fields name the session that observed the friction — the
session `/next-plan-review` must reach — while the `Landing ref` line names a
ref whose tree actually contains this Plan:
- Client: claude
- Conversation session ID: 616368aa-4f28-451e-802e-f98a84986c30
- Worktree/branch UUID: 9ccc2bc0-a8f0-4b07-8732-27723934b85c
- Session branch: claude/9ccc2bc0-a8f0-4b07-8732-27723934b85c
- Worktree: .claude\worktrees\BrokenEngine\9ccc2bc0-a8f0-4b07-8732-27723934b85c
- Landing ref: claude/9ccc2bc0-a8f0-4b07-8732-27723934b85c
- Run the review before /cleanup-worktrees removes the worktree recorded above:
  Codex transcript discovery requires the producing worktree to remain
  registered, and Claude review requires the exact conversation session ID
  above.

## Design
First root-cause the friction from the current tree and this Plan's `## Context`.
Only when the transcript is genuinely needed, in a new session run
`/next-plan-review claude/9ccc2bc0-a8f0-4b07-8732-27723934b85c` in bounded
friction mode, supplying the recorded client and conversation session ID. Then
make the smallest fix inside the `## In scope` boundary below.

The author's recommendation: add one line to
`.agents/references/scope-authorization.md` stating what a passing run reports
— a single `Decisive checks` row such as `Scope: authorization pass`, with no
per-region narration and no per-pass sub-bullets, and the mapping detail
written to a gitignored `Temp/` file cited under `Evidence` only when a later
worker needs it. Place it with the existing reporting sentences at `:25-26` so
the reference states both outcomes in one place, and add nothing to the
individual review skills, which already inherit this reference.

If root-causing shows the fix lies outside that boundary, surface it for
re-planning instead of expanding scope.

## Critical files
- `.agents/references/scope-authorization.md` — the three passes (`:12-23`) and
  the reporting sentences (`:25-26`)

## In scope
- Root-cause investigation as `## Design` states
- The smallest resulting fix, confined to the reporting statement in
  `.agents/references/scope-authorization.md`

## Out of scope
- The three passes themselves: what authorization, minimality, and KISS check,
  and the Tier-2+ applicability condition
- The `Scope: not supplied` failure report and the clause-citation requirement,
  which stay as written
- The review skills that inherit this reference, including
  `/coherence-review`, `/repo-code-review`, and `/glsl-review`
- `.agents/references/subagent-reporting.md` and the shared handoff form itself
- Unrelated skills/scripts; any transcript path or transcript text in the repo

## Risk tier and invariants
Expected Tier 2 (scoped behavior of one shared review reference); escalate if
the fix reaches build/bootstrap coordination. Invariants to preserve: a failing
pass still reports what it reports today, including `Scope: not supplied` and a
finding citing the violated clause; the reference stays one shared statement
rather than being copied into each review skill. Never embed transcript paths
or home paths.

## Acceptance criteria
- The reference states, in one place, both what a failing pass reports and what
  a passing pass reports
- A reviewer following it on a change with no scope finding returns one row and
  no per-region narration
- `/progressive-disclosure-review` passes on the changed file, and
  `/validate-skill` passes wherever the root AGENTS.md Apply the triggered
  cleanup step triggers it; plan validate exits 0
