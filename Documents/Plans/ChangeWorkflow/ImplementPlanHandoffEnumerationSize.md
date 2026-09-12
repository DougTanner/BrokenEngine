<!-- broken-engine-plan/v1 {"createdUtc":"2026-09-12T00:22:55.538Z","dependsOn":[]} -->
# Fix: /implement-plan — handoff enumeration rules produce a handoff past the shared row cap

## Context
Observed during this session's `/next-plan` run checkpoint (step 9), by the
`/next-plan-checkpoint-review` isolation/context-efficiency lens; its
measurement verdict was `needs-review` with one row over threshold and
`breachRowsTruncated` false.

- Tool: dispatched `implementer` running `/implement-plan` (slice B
  implementation handoff).
- Invocation: the standard `/implement-plan` dispatch for one approved plan
  slice.
- Measured size: 16933 characters, carrying 21 `Changed files` rows and 3
  exhaustive `Build required` rows.
- Emitting contract: `.agents/skills/implement-plan/SKILL.md` `## Handoff` —
  the "Name each changed file once" rule together with the per-`.cpp`
  `Build required` enumeration ("each changed `.cpp` names its exact target,
  configuration/platform, and selected project-member path") requires those
  enumerations inline, while the shared form in
  `.agents/references/subagent-handoff.md` `## Handoffs` caps any field at 10
  rows and requires the overflow to move to a file cited under `Evidence` as
  path plus selector. On a wide slice the two rules cannot both hold, so the
  handoff floods the main context with per-file rows main does not read there.
- Classification accepted at the checkpoint: fixable-defect in the emitter
  skill's handoff contract (skill text, not worker conduct — the worker
  followed the rules as written).

Session provenance (machine-local; not reproducible after cleanup). The Client
through Worktree fields name the session that observed the friction — the
session `/next-plan-review` must reach — while the `Landing ref` line names a
ref whose tree actually contains this Plan:
- Client: claude
- Conversation session ID: 374787de-14af-412c-a497-21dcd9400581
- Worktree/branch UUID: ce3b9527-1721-4141-b3ab-fdd16277d0ae
- Session branch: claude/ce3b9527-1721-4141-b3ab-fdd16277d0ae
- Worktree: .claude\worktrees\BrokenEngine\ce3b9527-1721-4141-b3ab-fdd16277d0ae
- Landing ref: claude/ce3b9527-1721-4141-b3ab-fdd16277d0ae — this Plan is
  recorded and landed by the session that observed the friction, so that
  branch's tip is its final commit and it survives exactly as long as the
  worktree recorded above.
  Fallback once the recorded ref is gone:
  `git log --follow --diff-filter=A --format=%H -- <this plan path>`, but a
  periodic Plan-history squash can make it return an unrelated aggregate
  commit, so review its result only when the commit is attributable to one
  session alone (its diff limited to that session's files); never review an
  aggregate or multi-session squash commit.
- Run the review before /cleanup-worktrees removes the worktree recorded above:
  Codex transcript discovery requires the producing worktree to remain
  registered, and Claude review requires the exact conversation session ID
  above.

## Design
First root-cause the friction from the current tree and this Plan's
`## Context`; the `## Context` citation already names both sides of the
conflict, so the transcript should not be needed. Only when it genuinely is, in
a new session run `/next-plan-review claude/ce3b9527-1721-4141-b3ab-fdd16277d0ae`
in bounded friction mode, supplying client `claude` and the recorded
conversation session ID.

The author's recommendation, which the accepted finding proposed, is to let the
`/implement-plan` handoff carry a count plus a `Temp/` file cited under
`Evidence` as path plus `##` selector in place of the inline changed-file and
build enumerations, keeping the shared 10-row cap authoritative: main derives
review scope from `.agents/scripts/Get-SessionChangeInventory.ps1` and briefs
the builder with targets only, so the per-file rows serve the next worker, not
main. The narrower alternative — stating only that the shared cap wins and the
overflow moves to a cited file — is available if the reviewer prefers not to
name a file shape in the skill. Either way the skill must stop requiring an
enumeration that the shared form forbids inline.

Make the smallest fix inside the `## In scope` boundary below. If root-causing
shows the fix lies outside that boundary — for example that the shared form
itself must change — surface it for re-planning instead of expanding scope.

## Critical files
- `.agents/skills/implement-plan/SKILL.md` (`## Handoff`)
- `.agents/references/subagent-handoff.md` (`## Handoffs`) — read as the
  authoritative shared form; changing it is out of scope

## In scope
- Root-cause investigation as `## Design` states
- The smallest resulting fix, confined to the `## Handoff` section of
  `.agents/skills/implement-plan/SKILL.md`: the `Build required` row form, the
  "Name each changed file once" rule, and the build-request sentence that
  requires per-`.cpp` enumeration

## Out of scope
- `.agents/references/subagent-handoff.md` and the shared form's caps
- `.agents/skills/compile/SKILL.md` (recorded separately) and any other skill
- `.agents/skills/implement-plan/references/worker.md` steps beyond whatever
  single reference a changed `## Handoff` rule makes incorrect
- The landed change this session produced
- Any transcript path or transcript text in the repo

## Risk tier and invariants
Tier 1 (mechanical): skill documentation shaping a handoff contract, with no
public signature or invariant exposure. Escalate only if the fix reaches the
shared handoff form or build/bootstrap coordination. Never embed transcript
paths or home paths. No unit tests.

## Acceptance criteria
- `.agents/skills/implement-plan/SKILL.md` `## Handoff` no longer requires an
  enumeration that would exceed the shared 10-row cap inline, and states the
  overflow route (count plus cited file with `##` selector) instead.
- `/external-skill-creator` validation of the changed skill package reports no
  new finding.
- The static-checks runner, invoked as `.agents/references/static-checks.md`
  documents it, reports every row the change triggers passing.

## Notes
Recorded as a context-efficiency follow-up from the `/next-plan` run checkpoint;
there was no unmet acceptance criterion — the cost is process efficiency.
