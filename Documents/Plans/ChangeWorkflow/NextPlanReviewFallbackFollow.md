<!-- broken-engine-plan/v1 {"createdUtc":"2026-09-06T14:45:02.772Z","dependsOn":[]} -->
# Fix: `/next-plan-review` — worker Step 5 restates the provenance fallback `git log` without `--follow`

## Context
While briefing a review of a landed change, main followed the provenance
fallback wording that `/next-plan-review` and the tooling-friction Plan template
share. The recorded landing ref no longer existed
(`git log -1 claude/c6070de5-edc9-436f-ac6c-c12a0a6d6dd2` returned
`fatal: ambiguous argument ... unknown revision`), so main ran the fallback in
its pre-`--follow` form,
`git log --diff-filter=A --format=%H -- <plan path>`, which returned
ba015fc4eccd15af1c5618e901eba761252a3f14 ("Sort plans into Engine, Game, Tools,
and ChangeWorkflow areas") — the aggregate commit that moved the Plan between
area directories, exactly the kind of commit the same sentence forbids
reviewing. Main had to re-derive the attributable commit by hand with
`git log --follow --diff-filter=A --format=%H -- <plan path>`, which returned
d6084a2f6460a77275fb89a44ce87035a8339a94, the observing session's own landing.
Cost: one blocked instruction and two extra main-session commands before the
review could be briefed.

The emitter is already correct: ba015fc4 added `--follow` to the fallback line
in
`.agents/skills/create-follow-up-plans/references/tooling-friction-plan-template.md:40`
and to the copies in the live Plans it touched. What that commit did not update
is the restatement in `.agents/skills/next-plan-review/references/worker.md:80`,
which still reads "the `git log --diff-filter=A` fallback commit". A reviewer
following that Step 5 sentence reproduces the same wrong result the moment a
Plan has been moved between area directories, and area sorting is a real
recurring maintenance operation.

Session provenance (machine-local; not reproducible after cleanup). The Client
through Worktree fields name the session that observed the friction — the
session `/next-plan-review` must reach — while the `Landing ref` line names a
ref whose tree actually contains this Plan:
- Client: claude
- Conversation session ID: 77755cf7-53a1-4d2e-b79f-b8debae0ecde
- Worktree/branch UUID: dc2d9566-3f25-4a45-a557-537825cb2a6a
- Session branch: claude/dc2d9566-3f25-4a45-a557-537825cb2a6a
- Worktree: .claude\worktrees\BrokenEngine\dc2d9566-3f25-4a45-a557-537825cb2a6a
- Landing ref: claude/dc2d9566-3f25-4a45-a557-537825cb2a6a
  Fallback once the recorded ref is gone:
  `git log --follow --diff-filter=A --format=%H -- Documents/Plans/ChangeWorkflow/NextPlanReviewFallbackFollow.md`,
  but a periodic Plan-history squash can make it return an unrelated aggregate
  commit, so review its result only when the commit is attributable to one
  session alone (its diff limited to that session's files); never review an
  aggregate or multi-session squash commit.
- Run the review before /cleanup-worktrees removes the worktree recorded above:
  Codex transcript discovery requires the producing worktree to remain
  registered, and Claude review requires the exact conversation session ID
  above.

## Design
First root-cause the friction from the current tree and this Plan's `## Context`.
Only when the transcript is genuinely needed, in a new session run
`/next-plan-review <review ref>` in bounded friction mode — the landing ref
above — supplying the recorded client and conversation session ID. Then make the
smallest fix inside the `## In scope` boundary below.

This author's recommendation for that fix, offered as a starting point rather
than a binding decision: add `--follow` to the command named in the attribution
bullet at `.agents/skills/next-plan-review/references/worker.md:80`, so the
sentence reads "the `git log --follow --diff-filter=A` fallback commit".
Rationale: it makes the reviewer's own instruction agree with the emitter the
same repository already fixed, and it is the smallest edit that stops the
sentence from naming a command that silently resolves to a rename commit.

Alternatives considered and not recommended: replacing the restatement with a
pointer to the template's fallback line, which trades a one-word fix for a
cross-skill reference the reviewer would have to follow mid-check; and leaving
the sentence alone on the grounds that the Plan's own copy now carries
`--follow`, which leaves the reviewer's instruction wrong for any Plan whose
recorded ref is gone and whose reviewer reads Step 5 rather than the Plan text.

If root-causing shows the fix lies outside that boundary, surface it for
re-planning instead of expanding scope.

## Critical files
- `.agents/skills/next-plan-review/references/worker.md` — the attribution
  bullet in Step 5 (currently `:79-81`); the authorized fix boundary
- `.agents/skills/create-follow-up-plans/references/tooling-friction-plan-template.md`
  — read-only: the emitter (currently `:39-44`) that already carries `--follow`
  and that the fixed sentence must agree with

## In scope
- Root-cause investigation as `## Design` states
- The smallest resulting prose fix, confined to the fallback command named in
  the Step 5 attribution bullet of
  `.agents/skills/next-plan-review/references/worker.md`

## Out of scope
- The tooling-friction Plan template and any other emitter of the fallback line;
  they already carry `--follow`
- A sweep of existing Plans' recorded fallback lines; every live Plan under
  `Documents/Plans/` already carries `--follow`
- Any change to how `/next-plan-review` selects or gates transcripts
- Unrelated skills/scripts; any transcript path or transcript text in the repo

## Risk tier and invariants
Expected Tier 1 (documentation wording, no public signature or invariant
exposure); escalate if root-causing shows the fix must change how a review
attributes a commit rather than which command it names. Never embed transcript
paths or home paths.

## Acceptance criteria
- The Step 5 attribution bullet names a fallback command that resolves a Plan's
  adding commit across a rename, so the recorded symptom no longer reproduces
- `/validate-skill` and `/progressive-disclosure-review` pass on the changed file
  where the root `AGENTS.md` Apply the triggered cleanup step triggers them
- `pwsh -NoProfile -File .agents/scripts/Test-PlanSchedulerState.ps1` reports
  `status: valid` and `code: ok`

## Coordination
No mandatory constraint binds this Plan to another live Plan. No live Plan names
`.agents/skills/next-plan-review/references/worker.md` in its scope sections.
