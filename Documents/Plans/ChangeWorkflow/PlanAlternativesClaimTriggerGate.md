<!-- broken-engine-plan/v1 {"createdUtc":"2026-09-10T11:26:18.012Z","dependsOn":[]} -->
# Fix: /plan-alternatives — `/next-plan` claim bullet states an ungated dispatch that contradicts the gated route

## Context
Observed during a `/next-plan` run on a documentation-only Plan. Three tracked
documents disagree about whether the `/plan-alternatives` dispatch from a
`/next-plan` claim is gated on a trigger:

- `.agents/skills/plan-alternatives/SKILL.md` `## When to use` bullet 2
  (lines 32-33) states the claim route with no gate: "From a `/next-plan` claim,
  after the preparation `implementer` verifies the Plan and before the Plan
  review reviewers." Bullet 1 of the same list (lines 27-31) does carry the
  gate, referencing `/plan-simplicity-review` `## When to use`.
- Root `AGENTS.md` Step 2 (Prepare and explore alternatives) gates the dispatch
  on `/plan-simplicity-review`'s trigger.
- `.agents/skills/next-plan/SKILL.md:97-99` says main runs `/plan-alternatives`
  after the preparation handoff "when its trigger fires".
- `.agents/skills/plan-simplicity-review/SKILL.md:27-35` owns that trigger: a
  plan step that adds new code or modifies non-documentation behavior.

Cost observed in the run: the claimed Plan was documentation-only, so the
trigger did not fire and `/plan-alternatives` was not dispatched. No repository
text resolves which of the two readings governs, so the run had to reconcile the
three documents by hand before it could justify not dispatching. A reader
following the ungated bullet would instead dispatch `researcher` workers on a
change that cannot benefit from them.

Session provenance (machine-local; not reproducible after cleanup). The Client
through Worktree fields name the session that observed the friction — the
session `/next-plan-review` must reach — while the `Landing ref` line names a
ref whose tree actually contains this Plan:
- Client: claude
- Conversation session ID: 3a28c34f-7b5f-4fa3-a3f0-e7a3a18518f6
- Worktree/branch UUID: 9dd7db24-08cd-4297-a4f2-09ce3526717e
- Session branch: claude/9dd7db24-08cd-4297-a4f2-09ce3526717e
- Worktree: .claude\worktrees\BrokenEngine\9dd7db24-08cd-4297-a4f2-09ce3526717e
- Landing ref: claude/9dd7db24-08cd-4297-a4f2-09ce3526717e
  Fallback once the recorded ref is gone:
  `git log --follow --diff-filter=A --format=%H -- <this plan path>`, but a periodic
  Plan-history squash can make it return an unrelated aggregate commit, so
  review its result only when the commit is attributable to one session alone
  (its diff limited to that session's files); never review an aggregate or
  multi-session squash commit.
- Run the review before /cleanup-worktrees removes the worktree recorded above:
  Codex transcript discovery requires the producing worktree to remain
  registered, and Claude review requires the exact conversation session ID
  above.

## Design
The three documents above are current text in this tree, so the contradiction is
readable without a transcript; root-cause it from the tree and this Plan's
`## Context` first. Only if the transcript is genuinely needed, in a new session
run `/next-plan-review claude/9dd7db24-08cd-4297-a4f2-09ce3526717e` in bounded
friction mode, supplying client `claude` and conversation session ID
`3a28c34f-7b5f-4fa3-a3f0-e7a3a18518f6`.

Recommended resolution, for the fix session to confirm or replace: treat root
`AGENTS.md` Step 2 and `.agents/skills/next-plan/SKILL.md:97-99` as the
authoritative reading — the dispatch is gated on `/plan-simplicity-review`'s
trigger on both routes — because the root workflow is the authoritative routing
policy and two of the three documents already agree with it. The recommended
edit is therefore confined to `.agents/skills/plan-alternatives/SKILL.md`
`## When to use`: make bullet 2 carry the same gate bullet 1 already carries, by
referencing `/plan-simplicity-review` `## When to use` rather than restating the
trigger, so the trigger keeps its single owner under the root progressive-
disclosure directive. Whether the two bullets are merged into one gated bullet
naming both entry points, or bullet 2 simply gains the reference, is left to the
fix session as the smaller edit of the two.

If root-causing shows the authoritative reading is instead the ungated one — the
claim route deliberately always explores alternatives — the corrective edit
lands in root `AGENTS.md` and `.agents/skills/next-plan/SKILL.md` instead, which
is outside the boundary below; surface that for re-planning rather than
expanding scope.

## Critical files
- `.agents/skills/plan-alternatives/SKILL.md` — `## When to use`, bullets 1-2
  (the authorized fix boundary)

## In scope
- Root-cause investigation as `## Design` states, over the four documents
  `## Context` cites
- The smallest resulting wording fix, confined to
  `.agents/skills/plan-alternatives/SKILL.md` `## When to use`, plus the
  matching phrase in that file's frontmatter `description` only if the edit
  leaves the description contradicting the body

## Out of scope
- Root `AGENTS.md`, `.agents/skills/next-plan/SKILL.md`, and
  `.agents/skills/plan-simplicity-review/SKILL.md`: the recommended resolution
  treats their text as already correct
- Changing the trigger itself, or where the trigger is owned
- Any other section of `.agents/skills/plan-alternatives/SKILL.md`
- The landed change the observing session produced
- Unrelated skills/scripts; any transcript path or transcript text in the repo

## Risk tier and invariants
Expected Tier 2 (scoped behavior of one skill's dispatch routing); escalate if
the fix reaches root `AGENTS.md` or another skill, which would span
independently owned documents. Never embed transcript paths or home paths.

## Acceptance criteria
- `.agents/skills/plan-alternatives/SKILL.md` `## When to use` states one
  consistent gating rule for both entry points, and a reader of that section
  alone reaches the same dispatch decision as a reader of root `AGENTS.md`
  Step 2
- The trigger's definition still lives only in
  `.agents/skills/plan-simplicity-review/SKILL.md` `## When to use`, referenced
  and not restated
- `/external-skill-creator` validate mode passes wherever the root `AGENTS.md`
  Apply the triggered cleanup step triggers it; plan validate exits 0
