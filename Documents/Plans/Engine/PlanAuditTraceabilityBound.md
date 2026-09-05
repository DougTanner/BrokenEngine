<!-- broken-engine-plan/v1 {"createdUtc":"2026-09-03T22:48:57.319Z","dependsOn":[]} -->
# Fix: `/plan-audit` — unbounded `Traceability checked:` field returns as a paragraph

## Context
Observed while running the Change Workflow Plan review step for a `/next-plan`
claim in this
session: the dispatched `reviewer` returned the `Traceability checked:` extension
field as a roughly 1,400-character single paragraph that mixed
requirement-to-check mappings, an importer census, and a walkthrough of how it
resolved the plan's citations. Main had to read the whole paragraph to extract
the mappings, which is the only part the field exists to deliver.

The emitter is `.agents/skills/plan-audit/SKILL.md:64`, which specifies the field
as `Traceability checked: <requirements/invariants <-> implementation
sites/checks>` with no bound on rows, length, or shape — unlike the per-finding
row at `:56`, which fixes an ID, severity, locator, and an
`evidence: repository-path:line` citation. `:69-70` then attaches these
extension fields to the shared handoff lines, whose rule in
`.agents/references/subagent-reporting.md` `## Handoffs` is "Every row is one
line", with the whole handoff capped at 40 lines or 20,000 characters and
oversized material moved to a file cited under `Evidence`. A field with no stated
shape does not inherit that rule visibly, so a reviewer can satisfy `SKILL.md`
and still breach the handoff contract, which is what the observed run did.

Session provenance (machine-local; not reproducible after cleanup). The Client
through Worktree fields name the session that observed the friction — the session
`/next-plan-review` must reach — while the `Landing ref` line names a ref whose
tree actually contains this Plan:
- Client: claude
- Conversation session ID: e992db79-2417-4700-bdf8-4ac6f0b5b498
- Worktree/branch UUID: a0377e9a-976d-4705-9ecc-b9a32413ba33
- Session branch: claude/a0377e9a-976d-4705-9ecc-b9a32413ba33
- Worktree: .claude\worktrees\BrokenEngine\a0377e9a-976d-4705-9ecc-b9a32413ba33
- Landing ref: claude/a0377e9a-976d-4705-9ecc-b9a32413ba33
  Fallback once the recorded ref is gone:
  `git log --diff-filter=A --format=%H -- Documents/Plans/Engine/PlanAuditTraceabilityBound.md`,
  but a periodic Plan-history squash can make it return an unrelated aggregate
  commit, so review its result only when the commit is attributable to one
  session alone (its diff limited to that session's files); never review an
  aggregate or multi-session squash commit.
- Run the review before /cleanup-worktrees removes the worktree recorded above:
  Codex transcript discovery requires the producing worktree to remain
  registered, and Claude review requires the exact conversation session ID above.

## Design
First root-cause the friction from the current tree and this Plan's `## Context`;
the cited section is visible without a transcript, so a transcript should not be
needed. Only when it is genuinely needed, in a new session run
`/next-plan-review claude/a0377e9a-976d-4705-9ecc-b9a32413ba33` in bounded
friction mode, supplying client `claude` and the conversation session ID above.

The author's recommendation, offered as a starting point rather than a binding
decision, is to give `:64` the same explicit shape the finding row at `:56`
already has: one row per requirement or invariant, each naming its implementation
site or check, with any supporting detail — an importer census, an unresolved or
ambiguous citation — cited as path plus selector rather than narrated, and
anything that still does not fit moved to a gitignored `Temp/` file cited under
the shared handoff's `Evidence` field, exactly as `## Handoffs` already directs
for oversized material. An alternative worth weighing at the Approve and
classify step is to state only
a row cap on the field and leave its internal shape to the auditor; it is a
smaller edit but it does not stop the narration that caused the observed
paragraph.

Then make the smallest resulting fix inside the `## In scope` boundary below. If
root-causing shows the fix lies outside that boundary — for example that the
shared handoff rule itself is what should change — surface it for re-planning
instead of expanding scope.

## Critical files
- `.agents/skills/plan-audit/SKILL.md`
- `.agents/references/subagent-reporting.md` (read-only authority for the handoff
  row and size rules)

## In scope
- Root-cause investigation as `## Design` states
- The smallest resulting fix, confined to `.agents/skills/plan-audit/SKILL.md`:
  the `Traceability checked:` line at `:64` inside the extension-field block at
  `:62-66`, and the sentence at `:69-70` attaching those fields to the shared
  handoff, only where it must name the new bound
- `.agents/skills/plan-audit/references/worker.md` only where it restates the
  changed field's shape

## Out of scope
- `.agents/references/subagent-reporting.md` `## Handoffs`, which is the
  authority this Plan conforms to, not a file this Plan changes
- The `API Verification Requests:` and `Required next step:` extension fields at
  `:63` and `:65`, and the per-finding row at `:56`
- The audit questions, categories, and dispatch rules
- `.agents/skills/plan-audit/SKILL.md:40-45` (the draft-card input) and
  `Test-PlanCitations.ps1`, both owned by
  `Documents/Plans/Engine/ExecutionCardFile.md`; that Plan and this one change
  disjoint sections of the same file
- `/plan-simplicity-review`'s paragraph-shaped finding entry, owned by
  `Documents/Plans/Engine/PlanSimplicityFindingEntryForm.md`
- Any handoff-format validator or script; this is a prose fix
- The landed change the observing session produced
- Unrelated skills/scripts; any transcript path or transcript text in the repo

## Risk tier and invariants
Expected Tier 2 (scoped tool behavior: the mandated output contract of one review
skill, which parents consume); this author's classification, for main to confirm
at the Approve and classify step. It drops to Tier 1 only if the resulting edit
provably changes no
content an auditor must emit. Invariants to preserve: the traceability judgment
itself — that every plan requirement and invariant maps to an implementation site
or check — still reaches the manager; the clean-result `PASS` statement and the
other two extension fields keep their current text; the audit stays findings-only
with no edits; no transcript path or home path is embedded.

## Acceptance criteria
- The recorded symptom no longer reproduces: following `SKILL.md` verbatim for an
  audit with several requirements yields a row-shaped `Traceability checked:`
  field within the shared handoff's size rule, with supporting detail cited
  rather than narrated
- `Validate-Skill.ps1` reports `VALID` for the changed `SKILL.md`; plan validate
  exits 0
