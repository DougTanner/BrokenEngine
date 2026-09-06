<!-- broken-engine-plan/v1 {"createdUtc":"2026-09-06T16:37:59.689Z","dependsOn":[]} -->
# Fix: root `AGENTS.md` — nothing tells main it need not read `.agents/references/static-checks.md` to brief the static pass

## Context

During a `/next-plan` run's Implement and propagate work, the main session read
`.agents/references/static-checks.md` in full and then composed the exact
`Invoke-StaticChecks.ps1` command into the brief it sent to the `implementer`
that runs the pass — while that same brief's `Governing paths` field already
named the same reference, which the dispatched `implementer` reads itself. The
file entered main's context for nothing main decided.

The emitter is the root `AGENTS.md` Change Workflow "Run targeted pre-review
checks" step, whose bullet reads:

> - an `implementer` runs the full applicable static pass in
>   `.agents/references/static-checks.md` after propagation — every tier.

That names the reference and the role, but says nothing about who reads the
file, so main reads it to be able to brief the run. Nothing else in the tree
says otherwise: `.agents/references/static-checks.md:1-10` opens by describing
the pass to its worker, and `.agents/skills/implement-plan/references/worker.md`
links it as worker material.

The content is small — about 2.3 KB, below the checkpoint's measured
oversized-result threshold — so the value of the fix is removing the pattern,
not the byte count. The same one-line habit repeats on every change at every
tier, since the bullet fires at every tier.

This is out of scope of the change the observing session landed: that change's
`## Out of scope` names the root `AGENTS.md` Change Workflow steps explicitly,
and `AGENTS.md` and `.agents/references/static-checks.md` are not in its changed
files.

Session provenance (machine-local; not reproducible after cleanup). The Client
through Worktree fields name the session that observed the friction — the
session `/next-plan-review` must reach — while the `Landing ref` line names a
ref whose tree actually contains this Plan:
- Client: claude
- Conversation session ID: b15337ed-cbeb-4e28-9536-e4cfd385d660
- Worktree/branch UUID: 4f413250-4e4e-4c0f-9c0f-c49aa01d4ab6
- Session branch: claude/4f413250-4e4e-4c0f-9c0f-c49aa01d4ab6
- Worktree: .claude\worktrees\BrokenEngine\4f413250-4e4e-4c0f-9c0f-c49aa01d4ab6
- Landing ref: claude/4f413250-4e4e-4c0f-9c0f-c49aa01d4ab6, this session's
  branch, whose landing commit will contain this Plan.
- Run the review before /cleanup-worktrees removes the worktree recorded above:
  Codex transcript discovery requires the producing worktree to remain
  registered, and Claude review requires the exact conversation session ID
  above.

## Design

First root-cause the friction from the current tree and this Plan's
`## Context`. Only when the transcript is genuinely needed, in a new session run
`/next-plan-review <landing ref>` in bounded friction mode, supplying the
recorded client and conversation session ID. Then make the smallest fix inside
the `## In scope` boundary below.

The author recommends one added clause on the existing root `AGENTS.md` bullet
quoted in `## Context`, saying in substance that main cites the reference in the
brief's `Governing paths` and the `implementer` reads it there, so main need not
read it.

Site judgment, against the root `AGENTS.md` progressive-disclosure directive:
the sentence belongs in the root `AGENTS.md` bullet, not in
`.agents/references/static-checks.md`'s opening. It is dispatch routing — which
session reads which file — which is the layer the root `AGENTS.md` owns, and it
must reach main before main opens the reference. A sentence placed in the
reference's own opening cannot prevent the read that reaches it, so it would
document the rule without fixing the friction. The Plan therefore names one
site: the "Run targeted pre-review checks" bullet in the root `AGENTS.md`.

Keep it to one clause on the existing bullet rather than a new bullet or
paragraph: the root `AGENTS.md` is the file every session loads, and the fact
being added is a single routing statement.

## Critical files

- `AGENTS.md` — the Change Workflow "Run targeted pre-review checks" step, its
  `implementer` bullet; the only file this change edits.
- `.agents/references/static-checks.md` — read-only: the reference the bullet
  names, whose opening already addresses its worker.
- `.agents/references/subagent-reporting.md` — read-only: `## Task brief`, which
  defines the `Governing paths` field the added clause relies on.

## In scope

- Root-cause investigation as `## Design` states.
- The smallest resulting prose fix, confined to the `implementer` bullet of the
  Change Workflow "Run targeted pre-review checks" step in the root
  `AGENTS.md`: add the clause stating that main cites
  `.agents/references/static-checks.md` in the brief's governing paths and the
  `implementer` reads it there.
- `/progressive-disclosure-review` on the changed file.

## Out of scope

- Every other step, bullet, and section of the root `AGENTS.md`.
- `.agents/references/static-checks.md`: its rows, its runner invocation, its
  envelope description, and its opening paragraph.
- `.agents/scripts/Invoke-StaticChecks.ps1` and every other script.
- `.agents/references/subagent-reporting.md` and every skill package.
- Which checks the static pass runs, when it runs, and which role runs it.
- The landed change the observing session produced.
- Any transcript path or transcript text in the repository; any C++, GLSL, or
  project-membership change.

## Risk tier and invariants

Expected Tier 1 under the root `AGENTS.md` risk tiers; this author's
classification, to be confirmed at the Approve and classify step. The trigger is
mechanical documentation work: one clause of routing prose, with no public
signature, no invariant, and no behavior of any script or skill exposed.
Escalate to Tier 2 if root-causing shows the fix must change which role runs the
pass or what the reference tells that role to run.

Invariants to preserve:
- The `implementer` still runs the full applicable static pass after
  propagation, at every tier, exactly as today.
- The reference stays the single owning layer for the pass's mechanics; the
  added clause states only who reads it, and restates no row of it.
- No fact is duplicated between the root `AGENTS.md` and
  `.agents/references/static-checks.md`.
- The changed file keeps its existing encoding and line endings; no transcript
  path or home path enters the repository.

## Acceptance criteria

- The root `AGENTS.md` "Run targeted pre-review checks" `implementer` bullet
  states that main cites `.agents/references/static-checks.md` in the brief and
  the `implementer` reads it, so main need not.
- The diff touches that bullet only, and `.agents/references/static-checks.md`
  is unchanged.
- `/progressive-disclosure-review` raises no duplication or layering finding
  against the added clause.
- `pwsh -NoProfile -File .agents/scripts/Test-PlanSchedulerState.ps1` reports
  `status: valid` and `code: ok`.
