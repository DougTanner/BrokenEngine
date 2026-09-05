<!-- broken-engine-plan/v1 {"createdUtc":"2026-09-05T22:21:38.723Z","dependsOn":[]} -->
# Fix: plan-alternatives SKILL.md — "one row each" set no length bound, so researcher rows returned as quote-laden paragraphs

## Context
Both `/plan-alternatives` researchers dispatched in this session returned their
extension fields as multi-sentence paragraphs rather than rows. `Mechanism`,
`Adds/Deletes`, `Critical files`, and `Pays off when` each ran to several
sentences, and the prose quoted `.agents/skills/plan-audit/SKILL.md:69-71`,
`.agents/references/subagent-reporting.md:135-137`, and
`.agents/skills/plan-simplicity-review/SKILL.md:96-114` inline. Main's five
comparison criteria at `.agents/skills/plan-alternatives/SKILL.md:64-72` need
none of those quotes verbatim: they score objective met, cause removed versus
symptom suppressed, and the rest from the candidate's own description. The
manager read the quoted material and discarded it.

`.agents/skills/plan-alternatives/SKILL.md:47-62` is the emitter. It says each
researcher returns the extension fields "one row each" and then shows the fenced
field list. "One row each" fixes how many rows a field gets; it sets no length
bound on a row and names no destination for supporting evidence, so a researcher
that wants to show its sourcing has nowhere to put it but the row.

`.agents/references/subagent-reporting.md:117-124` already states the general
rules — "Every row is one line", "Do not quote code", and the overflow route to
a gitignored `Temp/` file cited under `Evidence` as path plus selector — and
`:134-137` states that a declared extension field is "each one line or one row
per item, never a paragraph". So the general rules exist; what this Plan records
is that this skill's own "one row each" wording did not carry them to the
dispatched researcher.

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

The author's recommendation: bind the extension fields to one line each in the
same sentence that already says "one row each", and name the one place quoted or
supporting evidence may travel — a gitignored `Temp/` file cited under
`Evidence` as path plus selector — so a researcher that wants to show its
sourcing has a destination other than the row.
`.agents/skills/plan-audit/SKILL.md:69-71` is the recorded precedent for that
direction: "statements the audit checked and cleared stay in the auditor's own
context and travel only as a gitignored `Temp/` file cited under `Evidence`,
never as inline prose." Prefer reusing that wording over inventing a second
phrasing. Whether the same bound also needs stating in this skill's
`references/worker.md`, which the dispatched researcher reads, is for the fix
session to determine; do not duplicate the rule in both files.

If root-causing shows the fix lies outside that boundary, surface it for
re-planning instead of expanding scope.

## Critical files
- `.agents/skills/plan-alternatives/SKILL.md` — `## Handoff` (`:47-62`), the
  "one row each" sentence and the fenced field list
- `.agents/skills/plan-alternatives/references/worker.md` — the file the
  dispatched researcher reads; only a pointer belongs here if one is missing
- `.agents/references/subagent-reporting.md` — `## Handoffs` (`:95-137`),
  read-only; owns the one-line rule and the overflow route
- `.agents/skills/plan-audit/SKILL.md` — `:69-71`, read-only precedent for the
  destination wording

## In scope
- Root-cause investigation as `## Design` states
- The smallest resulting fix, confined to `## Handoff` in
  `plan-alternatives/SKILL.md`: the length bound on the extension fields and the
  named destination for supporting evidence, and, only if the investigation
  proves it necessary, one pointer to it in that skill's `references/worker.md`

## Out of scope
- Which extension fields exist and what each one means, including the
  no-viable-candidate row form
- The axes, the blind one-shot dispatch, the `### Comparison` criteria, and
  main's selection rule
- `## When to use` and the trigger for dispatching this skill
- `.agents/references/subagent-reporting.md` and the shared handoff form itself
- `.agents/skills/plan-audit/SKILL.md`, read-only precedent here
- The change this session landed in
  `.agents/skills/plan-simplicity-review/SKILL.md`
- Unrelated skills/scripts; any transcript path or transcript text in the repo

## Risk tier and invariants
Expected Tier 2 (scoped behavior of one skill's returned contract); escalate if
the fix reaches build/bootstrap coordination. Invariants to preserve: the field
names and their meanings stay unchanged, so main scores candidates on the same
five criteria; `Critical files` still carries 3-5 repository paths; a reported
empty axis still returns `Status: PASS` and is still not a finding;
`Build required: none` stays present and `Residuals` stays last. Never embed
transcript paths or home paths.

## Acceptance criteria
- `## Handoff`, read on its own, bounds each extension field to one line and
  names the single place supporting or quoted evidence may travel
- A researcher following it returns a candidate main can score without reading
  any quoted source text inline
- `/validate-skill` and `/progressive-disclosure-review` pass on the changed
  files; plan validate exits 0
