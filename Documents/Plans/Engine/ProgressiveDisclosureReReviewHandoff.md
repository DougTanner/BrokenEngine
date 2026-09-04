<!-- broken-engine-plan/v1 {"createdUtc":"2026-09-03T23:00:24.700Z","dependsOn":[]} -->
# Fix: progressive-disclosure-review SKILL.md — no declared handoff shape for a focused re-review

## Context
`.agents/skills/progressive-disclosure-review/SKILL.md:44-85` declares three
extension lines above `Findings` (`Skill:`, `Baseline:`, `Files checked:`), a
one-line `Findings` row form, and the four things a gating consumer must see. It
declares no shape for the focused re-review a manager dispatches after findings
are resolved, and two handoffs in this session drifted as a result:

- The first-round handoff appended a trailing free-prose paragraph headed
  "Note on judgment for the manager", which the shared form in
  `.agents/references/subagent-reporting.md` `## Handoffs` does not admit — that
  form extends only by rows inside an existing field or by declared extra
  fields, "each one line or one row per item, never a paragraph".
- The focused re-review handoff returned four multi-sentence
  `Verification notes:` paragraphs and omitted the shared handoff's
  `Decisive checks`, `Evidence`, `Executor`, and `Residuals` lines, so the
  manager could not read a per-ID verdict or the required closing lines.

Session provenance (machine-local; not reproducible after cleanup). The Client
through Worktree fields name the session that observed the friction — the
session `/next-plan-review` must reach — while the `Landing ref` line names a
ref whose tree actually contains this Plan:
- Client: claude
- Conversation session ID: 62f98e1f-3ac2-49b9-8c3a-08d631bb56db
- Worktree/branch UUID: ec8cd668-9c77-4241-bf54-16269ae03fa8
- Session branch: claude/ec8cd668-9c77-4241-bf54-16269ae03fa8
- Worktree: .claude\worktrees\BrokenEngine\ec8cd668-9c77-4241-bf54-16269ae03fa8
- Landing ref: claude/ec8cd668-9c77-4241-bf54-16269ae03fa8
- Run the review before /cleanup-worktrees removes the worktree recorded above:
  Codex transcript discovery requires the producing worktree to remain
  registered, and Claude review requires the exact conversation session ID
  above.

## Design
First root-cause the friction from the current tree and this Plan's `## Context`.
Only when the transcript is genuinely needed, in a new session run
`/next-plan-review claude/ec8cd668-9c77-4241-bf54-16269ae03fa8` in bounded
friction mode, supplying the recorded client and conversation session ID. Then
make the smallest fix inside the `## In scope` boundary below.

The author's recommendation: state in `## Handoff` that a focused re-review
returns the same three extension lines, the shared handoff's required lines
including `Decisive checks`, `Evidence`, `Executor`, and `Residuals`, and one
`Findings`-form row per re-checked ID carrying that ID's verdict — nothing else,
and no free-prose paragraph in either round. One added sentence about
re-checked-ID rows plus a sentence forbidding trailing prose is expected to be
enough; a separate re-review handoff template would restate the first-round form
and is not recommended.

If root-causing shows the fix lies outside that boundary, surface it for
re-planning instead of expanding scope.

## Critical files
- `.agents/skills/progressive-disclosure-review/SKILL.md` — `## Handoff`
  (`:44-85`)
- `.agents/references/subagent-reporting.md` — `## Handoffs`, read-only owner of
  the shared form and its extension rule

## In scope
- Root-cause investigation as `## Design` states
- The smallest resulting fix, confined to `## Handoff` in
  `progressive-disclosure-review/SKILL.md`: the re-review shape and the
  no-trailing-prose statement

## Out of scope
- The review's judgment content: the duplication/misplacement/size classes, the
  size thresholds, and the manager's decision standard
- The four gating requirements at `:70-81` and the `Baseline:` resolution rule
- `.agents/references/subagent-reporting.md` and the shared handoff form itself
- `references/worker.md`'s steps, unless a fact would otherwise be stated twice
- Unrelated skills/scripts; any transcript path or transcript text in the repo

## Risk tier and invariants
Expected Tier 2 (scoped behavior of one review skill's returned contract);
escalate if the fix reaches build/bootstrap coordination. Invariants to preserve:
the three declared extension lines keep their exact names and content, since a
consuming gate matches on them; `Changed files` and `Build required` stay `none`
for this findings-only review; `Build required` stays present and `Residuals`
stays last. Never embed transcript paths or home paths.

## Acceptance criteria
- `## Handoff` states the focused re-review shape, so a re-review handoff is
  checkable against the skill without inference
- Both rounds' handoffs are fully expressible as declared lines and rows, with no
  paragraph field available
- `/validate-skill` and `/progressive-disclosure-review` pass on the changed
  file; plan validate exits 0
