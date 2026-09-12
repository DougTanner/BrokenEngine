<!-- broken-engine-plan/v1 {"createdUtc":"2026-09-12T00:22:59.884Z","dependsOn":[]} -->
# Fix: /compile — build-reporting handoff extension restates narrowed rows as prose

## Context
Observed during this session's `/next-plan` run checkpoint (step 9), by the
`/next-plan-checkpoint-review` isolation/context-efficiency lens; its
measurement verdict was `needs-review` with one row over threshold and
`breachRowsTruncated` false.

- Tool: dispatched `builder` running `/compile` (this session's rebuild
  handoff).
- Invocation: the standard `/compile` dispatch for the session's rebuild.
- Measured size: the returned handoff carried a multi-paragraph build-reporting
  section ahead of the fenced shared form, restating per-build `status`,
  `exitCode`, `failureKind`, data mode, and retained-log paths that the same
  skill already narrows into `Decisive checks` and `Evidence`, so each build's
  facts reached main twice.
- Emitting contract: `.agents/skills/compile/SKILL.md` `## Handoff` declares an
  open "build reporting" extension as a bullet list of reporting duties, while
  the shared form in `.agents/references/subagent-handoff.md` `## Handoffs`
  allows a skill to extend the form only by adding rows inside an existing
  field or declaring extra fields, "each one line or one row per item, never a
  paragraph", with nothing preceding or following the fenced form.
- Classification accepted at the checkpoint: fixable-defect in the emitter
  skill's handoff contract (skill text, not worker conduct).

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

The author's recommendation, which the accepted finding proposed, is to bound
the declared extension to the rows the skill already narrows — the per-build
`Decisive checks` row (target, configuration, status, exitCode, failureKind),
the `Evidence` retained-log paths — plus this dispatch's envelope file cited as
path plus `##` selector (for example
`Temp/AgentBuildEnvelopes/compile-<stamp>.md` `## Compile dispatch ...`), so the
verbatim diagnostics live in the envelope and log rather than in prose ahead of
the fenced form. That follows the shared form's one-row-per-item extension rule.
Error diagnostics a failing build must surface inline should stay reachable;
whether they stay as rows inside an existing field or move entirely to the
cited envelope is the decision the fix session should settle from the shared
form's wording.

Make the smallest fix inside the `## In scope` boundary below. If root-causing
shows the fix lies outside that boundary — for example that the shared form
itself must change — surface it for re-planning instead of expanding scope.

## Critical files
- `.agents/skills/compile/SKILL.md` (`## Handoff`)
- `.agents/references/subagent-handoff.md` (`## Handoffs`) — read as the
  authoritative shared form; changing it is out of scope
- `.agents/skills/compile/references/worker.md` — only where a changed
  `## Handoff` rule makes its result discipline incorrect

## In scope
- Root-cause investigation as `## Design` states
- The smallest resulting fix, confined to the `## Handoff` section of
  `.agents/skills/compile/SKILL.md`: replacing the prose build-reporting
  bullets with row-shaped extensions plus the envelope file cited as path plus
  `##` selector, and the matching single reference in
  `.agents/skills/compile/references/worker.md` if one becomes incorrect

## Out of scope
- `.agents/references/subagent-handoff.md` and the shared form's caps
- `.agents/skills/implement-plan/SKILL.md` (recorded separately) and any other
  skill
- `.agents/skills/compile/scripts/` and the build driver's behavior, envelope
  schema, or retained-log policy
- The landed change this session produced
- Any transcript path or transcript text in the repo

## Risk tier and invariants
Tier 1 (mechanical): skill documentation shaping a handoff contract, with no
public signature or invariant exposure. Escalate only if the fix reaches the
shared handoff form, the build envelope schema, or build/bootstrap
coordination. Never embed transcript paths or home paths. No unit tests.

## Acceptance criteria
- `.agents/skills/compile/SKILL.md` `## Handoff` declares its extension as rows
  inside existing fields plus the envelope file cited as path plus `##`
  selector, with no paragraph-shaped reporting section and no field restating
  another.
- `/external-skill-creator` validation of the changed skill package reports no
  new finding.
- The static-checks runner, invoked as `.agents/references/static-checks.md`
  documents it, reports every row the change triggers passing.

## Notes
Recorded as a context-efficiency follow-up from the `/next-plan` run checkpoint;
there was no unmet acceptance criterion — the cost is process efficiency.
