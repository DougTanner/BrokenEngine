<!-- broken-engine-plan/v1 {"createdUtc":"2026-09-05T22:22:29.212Z","dependsOn":[]} -->
# Fix: plan-alternatives — the researcher brief hands over the claimed Plan path, and a researcher read the whole Plan

## Context
`.agents/skills/plan-alternatives/references/worker.md:39` requires each
researcher to work "blind, from its own brief alone", and the root `AGENTS.md`
Prepare and explore alternatives step lists exactly what the brief carries —
objective, the plan's `## In scope`/`## Out of scope`, evidence paths, tier,
fixed user decisions, candidate zero, the assigned axis — and ends "never the
plan's rationale". That recipe never asks for the Plan's own path.

In the observed `/next-plan` run, main's brief nonetheless named the claimed Plan
path, and named it only inside a negative exclusion of the form "do not read
<Plan path> beyond the scope sections quoted here". Handing a worker a path is
handing it the file: the axis-2 `researcher`'s own handoff `Residuals` stated it
had read the claimed Plan in full before noticing the brief limited it to the
quoted scope sections, so it decided its candidate with the Plan's rationale in
context — precisely what the recipe withholds. The axis-1 researcher stayed
blind, so the two candidates main compared were not produced under the same
conditions.

`.agents/skills/plan-alternatives/references/worker.md:37` makes this
unrecoverable in the run: "Each researcher returns
once. There is no second round." Main could not re-run the axis, so it compared
a contaminated candidate against a blind one and against candidate zero. The
rework was absorbed by main's comparison rather than by a repeated dispatch.

`.agents/skills/plan-alternatives/references/worker.md:8` is the mechanism that turns the mention into a read: step 1 is
"Read the brief and the evidence paths it cites", and a path in the brief reads
as a cited path regardless of the sentence around it.

Session provenance (machine-local; not reproducible after cleanup). The Client
through Worktree fields name the session that observed the friction — the
session `/next-plan-review` must reach — while the `Landing ref` line names a
ref whose tree actually contains this Plan:
- Client: claude
- Conversation session ID: c9c5b848-9887-430c-9644-2264fda3c219
- Worktree/branch UUID: 9505f144-de8a-4a63-9493-ffe2b804e71d
- Session branch: claude/9505f144-de8a-4a63-9493-ffe2b804e71d
- Worktree: .claude\worktrees\BrokenEngine\9505f144-de8a-4a63-9493-ffe2b804e71d
- Landing ref: claude/9505f144-de8a-4a63-9493-ffe2b804e71d
- Run the review before /cleanup-worktrees removes the worktree recorded above:
  Codex transcript discovery requires the producing worktree to remain
  registered, and Claude review requires the exact conversation session ID
  above.

## Design
First root-cause the friction from the current tree and this Plan's `## Context`.
Only when the transcript is genuinely needed, in a new session run
`/next-plan-review claude/9505f144-de8a-4a63-9493-ffe2b804e71d` in bounded
friction mode, supplying the recorded client and conversation session ID. Then
make the smallest fix inside the `## In scope` boundary below.

The author's recommendation is to make the blind condition self-enforcing at the
two places that produce and consume the brief, rather than relying on a
researcher honouring an exclusion:

- Root `AGENTS.md` Prepare and explore alternatives step: state that the brief
  carries the two scope sections as quoted text and does not carry the plan
  file's path in any form, positive or negative. This removes the only channel
  the observed leak used, and it costs one clause in a recipe that already
  enumerates the brief's fields.
- `plan-alternatives/references/worker.md`: bound step 1's reading to the brief
  text plus the evidence paths, and state under `## Rules` that a plan file is
  not an evidence path even when the brief mentions one — the worker stops at
  the quoted scope sections. This is the belt to the recipe's braces, and it
  covers a brief written by an older recipe or by a caller who improvises.

Both edits are wording; neither adds a script, a check, or a field. The author
considered instead adding a validation step that inspects briefs before dispatch
and rejects one containing a Plan path, and recommends against it: it is new
machinery for a case two sentences remove, and nothing in the repository
inspects brief text today.

If root-causing shows the fix lies outside that boundary, surface it for
re-planning instead of expanding scope.

## Critical files
- `AGENTS.md` — the Prepare and explore alternatives step's `/plan-alternatives`
  brief recipe bullet, which enumerates the brief's fields
- `.agents/skills/plan-alternatives/references/worker.md` — step 1 (`:8-9`) and
  `## Rules` (`:37-39`), the one-shot rule and the blind condition
- `.agents/skills/plan-alternatives/SKILL.md` — `## Inputs`, read-only unless the
  recipe's location statement stops being true after the fix

## In scope
- Root-cause investigation as `## Design` states
- The smallest resulting fix, confined to: the root `AGENTS.md`
  `/plan-alternatives` brief-recipe bullet, so the brief supplies the scope
  sections as quoted text and no Plan path in any form; and step 1 plus
  `## Rules` in `plan-alternatives/references/worker.md`, so a researcher's
  reading is bounded to the brief text and the evidence paths, with a plan file
  excluded from what counts as an evidence path
- A matching wording correction inside `plan-alternatives/SKILL.md` `## Inputs`
  only if the fix makes its statement about where the recipe lives inaccurate

## Out of scope
- The axes themselves, the one-shot dispatch rule, the handoff extension fields,
  the comparison criteria, and the user-presentation gate in
  `plan-alternatives/SKILL.md`
- `/next-plan` claim selection, the resolved-snapshot route a chosen alternative
  takes, and `.agents/skills/next-plan/**`
- `.agents/references/subagent-reporting.md` and the shared task-brief form
- Re-running the observed run's axes or revisiting the candidate main selected
- The landed change the session produced
- Unrelated skills/scripts; any transcript path or transcript text in the repo

## Risk tier and invariants
Expected Tier 2 (scoped behavior of one skill's dispatch contract); the trigger
is a change to how a dispatched worker is briefed and bounded. Escalate if the
fix reaches build/bootstrap coordination. Invariants to preserve: the researcher
still receives both scope sections in full, so its candidate is still measured
against the same boundary; the axis set, the one-shot rule, and the handoff
fields stay unchanged; and the recipe stays in root `AGENTS.md` with the skill
referencing it, per progressive disclosure. Never embed transcript paths or home
paths.

## Acceptance criteria
- Reading the root `AGENTS.md` recipe alone, a main session composing a brief has
  no field that would carry the plan file's path, and the two scope sections
  travel as quoted text
- Reading `plan-alternatives/references/worker.md` alone, a researcher handed a
  brief that does mention a plan path does not read that file
- `/validate-skill` and `/progressive-disclosure-review` pass on the changed
  files; plan validate exits 0
