<!-- broken-engine-plan/v1 {"createdUtc":"2026-09-05T21:27:48.793Z","dependsOn":[]} -->
# Fix: `/create-follow-up-plans` — `## Inputs` does not say the provenance block is supplied only for tooling-friction and isolation follow-ups

## Context
Observed at one `/next-plan` run's checkpoint, in the run that claimed
`Documents/Plans/Engine/PackChunkLoaderResetExclusion.md`.

`.agents/skills/create-follow-up-plans/SKILL.md:22-28` (`## Inputs`) lists
`Objective`, `Scope`, `Fixed decisions`, and `Evidence`, and says nothing about
session provenance. The provenance block itself appears only in
`.agents/skills/create-follow-up-plans/references/tooling-friction-plan-template.md`,
which is the private worker-side file a dispatching parent does not read, and
`.agents/skills/next-plan/references/follow-up-provenance.md:3-4` scopes the
provenance values to a "tooling-friction or context-efficiency follow-up". The
root `AGENTS.md` Verify the acceptance table step routes every proven
out-of-scope leftover to this same skill without drawing that distinction, so a
parent reading `SKILL.md` alone has nothing telling it that an ordinary debt
follow-up needs no provenance.

What that missing clause produced in the observed run: for an ordinary debt
follow-up (an engine `.pack` file-size reduction, no tooling friction involved),
main spent one shell call reading `CLAUDE_CODE_SESSION_ID` and wrote a
provenance section into the dispatch brief. The dispatched `implementer`
correctly did not put those values in the Plan, and instead returned a residual
in its handoff explaining that a debt Plan carries no provenance block — so the
shell call, the brief section, and the residual were all rework produced by the
unstated scope.

Session provenance (machine-local; not reproducible after cleanup). The Client
through Worktree fields name the session that observed the friction — the
session `/next-plan-review` must reach — while the `Landing ref` line names a
ref whose tree actually contains this Plan:
- Client: claude
- Conversation session ID: 7bbceba3-9f2d-41e9-8efd-5d44d90fda63
- Worktree/branch UUID: 0c3c3845-ec0f-4533-af6b-469c0303a5e6
- Session branch: claude/0c3c3845-ec0f-4533-af6b-469c0303a5e6
- Worktree: .claude\worktrees\BrokenEngine\0c3c3845-ec0f-4533-af6b-469c0303a5e6
- Landing ref: claude/0c3c3845-ec0f-4533-af6b-469c0303a5e6, this session's
  branch, whose landing commit will contain this Plan.
  Fallback once the recorded ref is gone:
  `git log --follow --diff-filter=A --format=%H -- Documents/Plans/ChangeWorkflow/FollowUpProvenanceInputScope.md`,
  but a periodic Plan-history squash can make it return an unrelated aggregate
  commit, so review its result only when the commit is attributable to one
  session alone (its diff limited to that session's files); never review an
  aggregate or multi-session squash commit.
- Run the review before /cleanup-worktrees removes the worktree recorded above:
  Codex transcript discovery requires the producing worktree to remain
  registered, and Claude review requires the exact conversation session ID
  above.

## Design
First root-cause the friction from the current tree and this Plan's
`## Context`; the gap is visible in the cited files without any transcript. Only
when the transcript is genuinely needed, in a new session run
`/next-plan-review claude/0c3c3845-ec0f-4533-af6b-469c0303a5e6` in bounded
friction mode, supplying the recorded client and the conversation session ID
recorded above. Then make the smallest fix inside the `## In scope` boundary
below.

This author's recommendation for that fix, offered as a starting point rather
than a binding decision: add one input clause to `## Inputs` in
`.agents/skills/create-follow-up-plans/SKILL.md` naming session provenance as an
input supplied only for tooling-friction and context-efficiency follow-ups, and
not for an ordinary debt follow-up. Rationale: `## Inputs` is the public list a
dispatching parent reads to assemble its brief, the scoping fact already exists
in `.agents/skills/next-plan/references/follow-up-provenance.md` and only needs
to be reachable from the public file, and one clause removes both the wasted
lookup and the residual without moving any field definitions out of the template
that owns them.

Alternatives considered and not recommended: restating the provenance block's
fields in `SKILL.md`, which would duplicate the template that owns them and
violate the root `AGENTS.md` progressive-disclosure directive; and changing the
root `AGENTS.md` routing sentence instead, which would put a skill-local input
detail in the file that carries only routing.

If root-causing shows the fix lies outside the boundary below — for example that
`.agents/skills/next-plan/SKILL.md` is the file that misroutes the provenance —
surface it for re-planning instead of expanding scope.

## Critical files
- `.agents/skills/create-follow-up-plans/SKILL.md` — `## Inputs` (currently
  `:22-28`), the authorized fix boundary
- `.agents/skills/create-follow-up-plans/references/tooling-friction-plan-template.md`
  — read-only: the file that owns the provenance block's field shapes
- `.agents/skills/next-plan/references/follow-up-provenance.md` — read-only: the
  existing statement (currently `:3-4`) scoping provenance to tooling-friction
  and context-efficiency follow-ups

## In scope
- Root-cause investigation as `## Design` states
- The smallest resulting prose fix, confined to `## Inputs` in
  `.agents/skills/create-follow-up-plans/SKILL.md`, stating for which follow-up
  categories session provenance is supplied and that ordinary debt follow-ups
  take none

## Out of scope
- The field shapes, wording, and structure of the provenance block in
  `.agents/skills/create-follow-up-plans/references/tooling-friction-plan-template.md`
- `.agents/skills/create-follow-up-plans/references/worker.md`, including its
  steps, its tooling-friction substitution rules, and its duplicate rules
- `## Purpose`, `## When to use`, `## Handoff`, `## References`, and the
  frontmatter of `.agents/skills/create-follow-up-plans/SKILL.md`
- `.agents/skills/next-plan/**`, including `follow-up-provenance.md` and the
  claim-exit step
- Root `AGENTS.md`, `Documents/AGENTS.md`, and `Documents/Plans/AGENTS.md`
- Adding any script; the landed change the observing session produced
- Unrelated skills/scripts; any transcript path or transcript text in the repo

## Risk tier and invariants
Expected Tier 1 (mechanical documentation change to one skill's public input
list, with no behavior or contract change) under the root `AGENTS.md` risk
tiers; this author's classification, to be confirmed at the Approve and classify
step. The trigger is a prose-only change to a skill package file. Escalate if
the investigation shows the fix must change what a worker does rather than what
a parent supplies.

Invariants to preserve:
- Tooling-friction and context-efficiency follow-ups still receive full
  provenance, and their Plans still carry the block the template defines
- The provenance block's field definitions stay in the template file alone; the
  public file references the category, not the fields
- `SKILL.md` remains the public file, carrying only what a parent needs to
  dispatch and consume the skill
- The file keeps its existing encoding and line endings; no transcript path or
  home path enters the repository

## Acceptance criteria
- `## Inputs`, read on its own, tells a dispatching parent whether a given
  follow-up needs session provenance, without opening any reference file
- A dispatch brief for an ordinary debt follow-up assembled from that section
  contains no provenance section and needs no `CLAUDE_CODE_SESSION_ID` lookup
- The diff touches only the file named in `## In scope`
- `/validate-skill` and `/progressive-disclosure-review` pass on the changed file
  where the root `AGENTS.md` Apply the triggered cleanup step triggers them
- `pwsh -NoProfile -File .agents/scripts/Test-PlanSchedulerState.ps1` reports
  `status: valid` and `code: ok`

## Coordination
No mandatory constraint binds this Plan to another live Plan. No live Plan names
`.agents/skills/create-follow-up-plans/SKILL.md`;
`Documents/Plans/ChangeWorkflow/DeadPlanClosureReferenceScript.md` places any change to
`.agents/skills/create-follow-up-plans/references/worker.md` out of its own
scope, and this Plan does not touch that file either, so the two are disjoint.
