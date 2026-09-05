<!-- broken-engine-plan/v1 {"createdUtc":"2026-09-05T22:22:17.860Z","dependsOn":[]} -->
# Fix: implement-plan — the worker returns the same-context audit as prose instead of the declared `Self-audit resolved` handoff line

## Context
In this `/next-plan` run, the dispatched `/implement-plan` worker returned a
handoff whose block omitted the declared extension field
`Self-audit resolved: <Claim -> Check -> Result; fix/recheck, or none>`
(`.agents/skills/implement-plan/SKILL.md:54`). In its place the worker appended a
six-item prose narrative headed "Same-context audit (Phase 2)" outside the
handoff block. `.agents/references/subagent-reporting.md:134-136` allows a skill
to extend the shared form "only by adding rows inside an existing field or by
declaring extra fields in its own `## Handoff` section, each one line or one row
per item, never a paragraph", so the returned shape violated the form the skill
declares. Main had to read the prose narrative and map it back onto the missing
field itself before it could route the result.

The likely cause is visible in the current tree: Phase 2 of
`.agents/skills/implement-plan/references/worker.md:54-80` — the private file the
dispatched worker actually reads — describes the same-context audit as a ranked
Claim/Check/Result list (`:56-59`) and requires the handoff to reflect audit
fixes (`:75-78`), but it never names the `Self-audit resolved` field those
results belong in. A repository-wide search for the literal string returns only
`.agents/skills/implement-plan/SKILL.md:54`,
`.agents/skills/resolve-findings/SKILL.md:59`, and
`.agents/skills/resolve-findings/references/worker.md:22`. The
`/resolve-findings` worker file names the field at its audit step ("Done when the
result is reported under `Self-audit resolved`"); the `/implement-plan` worker
file has no equivalent sentence, so a worker following the private file has
nothing pointing its audit output at the declared line. Confirm this reading in
the fix session before changing anything.

Session provenance (machine-local; not reproducible after cleanup). The Client
through Worktree fields name the session that observed the friction — the
session `/next-plan-review` must reach — while the `Landing ref` line names a
ref whose tree actually contains this Plan:
- Client: claude
- Conversation session ID: ba3b6fe9-747d-48f6-ac0a-b5d3a9e04137
- Worktree/branch UUID: fcee32ed-cb72-40cd-8b79-32b52cb70856
- Session branch: claude/fcee32ed-cb72-40cd-8b79-32b52cb70856
- Worktree: .claude\worktrees\BrokenEngine\fcee32ed-cb72-40cd-8b79-32b52cb70856
- Landing ref: claude/fcee32ed-cb72-40cd-8b79-32b52cb70856
  Fallback once the recorded ref is gone:
  `git log --diff-filter=A --format=%H -- <this plan path>`, but a periodic
  Plan-history squash can make it return an unrelated aggregate commit, so
  review its result only when the commit is attributable to one session alone
  (its diff limited to that session's files); never review an aggregate or
  multi-session squash commit.
- Run the review before /cleanup-worktrees removes the worktree recorded above:
  Codex transcript discovery requires the producing worktree to remain
  registered, and Claude review requires the exact conversation session ID
  above.

## Design
First root-cause the friction from the current tree and this Plan's `## Context`.
Only when the transcript is genuinely needed, in a new session run
`/next-plan-review claude/fcee32ed-cb72-40cd-8b79-32b52cb70856` in bounded
friction mode, supplying the recorded client `claude` and the recorded
conversation session ID above. Then make the smallest fix inside the
`## In scope` boundary below.

The author's recommendation, if root-causing confirms the reading above, is the
one-line pointer `/resolve-findings` already uses: name the `Self-audit resolved`
field in the Phase 2 done-condition of
`.agents/skills/implement-plan/references/worker.md`, so the worker running the
audit sees the field its results must fold into, and add nothing else. Restating
the field's shape in the worker file would duplicate `SKILL.md:54` and is not
recommended. If root-causing shows the fix lies outside the boundary below —
for example that the shared form in `.agents/references/subagent-reporting.md`
is the site that must change — surface it for re-planning instead of expanding
scope.

## Critical files
- `.agents/skills/implement-plan/references/worker.md`
- `.agents/skills/implement-plan/SKILL.md`

## In scope
- Root-cause investigation as `## Design` states
- The smallest resulting fix, confined to the two files named above — in
  `references/worker.md` the Phase 2 same-context audit section, and in
  `SKILL.md` the `## Handoff` section only if the investigation proves the
  declaration itself is the defect

## Out of scope
- The landed change the session produced
- `.agents/references/subagent-reporting.md` and the shared handoff form
- `/resolve-findings` and every other skill; any transcript path or transcript
  text in the repo

## Risk tier and invariants
Expected Tier 1 (mechanical instruction-prose change with no behavior or
signature exposure); escalate to Tier 2 if the fix turns out to change what the
skill does rather than where the worker is told to report. Never embed
transcript paths or home paths. Progressive disclosure holds: the field's shape
stays declared once at `SKILL.md:54` and is referenced, not restated.

## Acceptance criteria
- A dispatched `/implement-plan` worker returns the audit result on the declared
  `Self-audit resolved` line inside the handoff block, with no prose narrative
  outside it
- /validate-skill passes on the `implement-plan` package;
  `pwsh -NoProfile -File .agents/scripts/Test-PlanSchedulerState.ps1` reports
  `status: valid` / `code: ok`
