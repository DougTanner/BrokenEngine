<!-- broken-engine-plan/v1 {"createdUtc":"2026-09-05T22:21:32.752Z","dependsOn":[]} -->
# Fix: implement-plan SKILL.md — a returned handoff carried a prose preamble, two status tokens, sub-bullets, and no Evidence or Executor row

## Context
An `/implement-plan` worker dispatched in this session returned a handoff whose
form the manager had to repair before reading it. In one return:
- a prose preamble ("Implementation complete.") sat above the handoff;
- the status line read `Status: NEEDS_ACTION — no; complete. **Status: OK**`,
  carrying two status tokens, neither one of the three the shared form allows,
  plus commentary on the same line;
- an undeclared `Skill:` line appeared, which this skill declares nowhere;
- `Decisive checks` used multi-level sub-bullets instead of one row per check;
- `Self-audit resolved` was a four-bullet block, where the skill declares it as
  one line;
- no `Evidence:` row and no `Executor:` row were returned at all.

`.agents/skills/implement-plan/SKILL.md:47-68` is the emitter. Its `## Handoff`
points at the shared form and then shows a fenced block (`:53-62`) listing the
extension fields plus `Build required` and `Residuals`. That fence repeats two
shared fields and omits the rest — `Status`, `Findings`, `Changed files`,
`Decisive checks`, `Evidence`, `Executor` — so a worker reading the fence as the
handoff it must produce drops the omitted shared rows and has no visible
one-token `Status` form to copy. The prose after the fence at `:64-68` names
`Residuals` last and one row per changed file, but sets no cap on any field and
names no destination for detail that will not fit.

The shared form at `.agents/references/subagent-reporting.md:95-137` already
states each of the violated rules: the three `Status` tokens at `:101`, "Every
row is one line" and "Do not quote code and do not repeat a row from another
field" at `:117-118`, the `Evidence` and `Executor` rows at `:106-107`, the
overflow route to a gitignored `Temp/` file cited under `Evidence` at `:118-124`,
and at `:134-137` that a skill may extend the form only by rows inside an
existing field or by declared extension fields, "each one line or one row per
item, never a paragraph". So the rules exist; what this Plan records is that
following this skill's own `## Handoff` did not produce them.

Session provenance (machine-local; not reproducible after cleanup). The Client
through Worktree fields name the session that observed the friction — the
session `/next-plan-review` must reach — while the `Landing ref` line names a
ref whose tree actually contains this Plan:
- Client: claude
- Conversation session ID: 8ef0714e-d5fa-48b2-a4a8-00788620d328
- Worktree/branch UUID: c5715c35-1334-459b-a278-e8aa1ae47572
- Session branch: claude/c5715c35-1334-459b-a278-e8aa1ae47572
- Worktree: .claude\worktrees\BrokenEngine\c5715c35-1334-459b-a278-e8aa1ae47572
- Landing ref: claude/c5715c35-1334-459b-a278-e8aa1ae47572
- Run the review before /cleanup-worktrees removes the worktree recorded above:
  Codex transcript discovery requires the producing worktree to remain
  registered, and Claude review requires the exact conversation session ID
  above.

## Design
First root-cause the friction from the current tree and this Plan's `## Context`.
Only when the transcript is genuinely needed, in a new session run
`/next-plan-review claude/c5715c35-1334-459b-a278-e8aa1ae47572` in bounded
friction mode, supplying the recorded client and conversation session ID. Then
make the smallest fix inside the `## In scope` boundary below.

The author's recommendation: make `## Handoff` state, in the section a worker
reads, that the return is the complete shared handoff with these extension
fields added — not the fenced block alone — that `Status` is exactly one of the
three shared tokens on a line with nothing else, that `Self-audit resolved` is
one line, and that detail exceeding a row travels to a gitignored `Temp/` file
cited under `Evidence` as path plus selector rather than becoming sub-bullets or
a preamble. `.agents/skills/plan-audit/SKILL.md:69-71` is the recorded precedent
for that last direction: "statements the audit checked and cleared stay in the
auditor's own context and travel only as a gitignored `Temp/` file cited under
`Evidence`, never as inline prose." Prefer reusing that wording, and prefer
removing the fence's duplication of shared fields over restating the whole
shared form here, so the two files cannot drift. Whether the fix belongs partly
in this skill's `references/worker.md`, which the dispatched worker reads end to
end, is for the fix session to determine; do not duplicate the same rule in both
files.

If root-causing shows the fix lies outside that boundary, surface it for
re-planning instead of expanding scope.

## Critical files
- `.agents/skills/implement-plan/SKILL.md` — `## Handoff` (`:47-68`), the fenced
  extension-field block (`:53-62`) and the prose after it
- `.agents/skills/implement-plan/references/worker.md` — the file the dispatched
  worker reads; only a pointer belongs here if one is missing
- `.agents/references/subagent-reporting.md` — `## Handoffs` (`:95-137`),
  read-only; owns the shared fields, the one-line rule, and the overflow route
- `.agents/skills/plan-audit/SKILL.md` — `:69-71`, read-only precedent for the
  destination wording

## In scope
- Root-cause investigation as `## Design` states
- The smallest resulting fix, confined to `## Handoff` in
  `implement-plan/SKILL.md` — what the fenced block represents, the `Status`
  form, the one-line bound on `Self-audit resolved` and the other extension
  fields, and where overflow detail travels — and, only if the investigation
  proves it necessary, one pointer to it in that skill's `references/worker.md`

## Out of scope
- The work the skill performs: the implementation steps, the same-context
  self-audit, and the slice contract
- Which extension fields exist and what each one means; this Plan bounds their
  form, not their content
- `.agents/references/subagent-reporting.md` and the shared handoff form itself
- `.agents/skills/plan-audit/SKILL.md`, read-only precedent here
- Other skills' `## Handoff` sections, each owned by its own record
- The change this session landed in
  `.agents/skills/plan-simplicity-review/SKILL.md`
- Unrelated skills/scripts; any transcript path or transcript text in the repo

## Risk tier and invariants
Expected Tier 2 (scoped behavior of one implementation skill's returned
contract); escalate if the fix reaches build/bootstrap coordination. Invariants
to preserve: the declared extension fields keep their names and meanings, so a
manager routes the same build and propagation work; `Build required` requests
stay executable without rediscovery, naming target, configuration/platform, and
selected project-member path; `Build required` stays present and `Residuals`
stays last. Never embed transcript paths or home paths.

## Acceptance criteria
- `## Handoff`, read on its own, yields a return carrying every shared field
  including `Evidence` and `Executor`, with one `Status` token and no preamble
- Each extension field, `Self-audit resolved` included, has a stated one-line
  bound and a stated destination for detail that exceeds it
- `/validate-skill` and `/progressive-disclosure-review` pass on the changed
  files; plan validate exits 0
