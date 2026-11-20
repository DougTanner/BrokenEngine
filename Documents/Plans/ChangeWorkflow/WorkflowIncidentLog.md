<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-20T22:14:31.730Z","dependsOn":[]} -->
# Record workflow friction as incidents and plan only on the third

## Context

Every `/next-plan` run on Claude ends with `/next-plan-checkpoint-review`
(dispatched per `.agents/skills/next-plan/references/run-checkpoint.md`
`## Dispatch`), which returns friction, context-efficiency, and isolation
findings. `run-checkpoint.md` `## Follow-up routing` sends each accepted
finding through the Change Workflow Leftovers rule
(`.agents/references/change-workflow.md` `#### Step 8 — Verify the acceptance
table`), so a single observation becomes either a fix inside the current change
or a tooling-friction Plan written by `/create-follow-up-plans`
(`.agents/skills/create-follow-up-plans/SKILL.md` `## When to use`, bullet "A
tooling-friction follow-up is recorded at a `/next-plan` claim exit").
Friction first seen after the checkpoint, and the workflow-friction and
token-efficiency findings of `/next-plan-review` that main accepts, take the
same path. No rule requires a problem to recur before workflow text changes,
and workflow prose has grown every week for 15 weeks.

The user's decision: collect these incidents in a `Documents/` sub-folder and
create a change Plan only once a similar problem has been hit three or more
times. The user also said the workflow "is producing good results", so the
mechanism stays small.

Answering choices this Plan's draft presented, the user made two more
decisions. Asked whether a script or skill that produced a wrong result skips
the count, the user replied "Yes, objectively wrong wording or bugs should
always be fixed", which widens the exemption to an objectively wrong
instruction statement as well as a bug (Design item 6). For the third similar
incident, the user picked the presented option "always a Plan through
/create-follow-up-plans, never a fix inside the current change" (item 5).

The repository already has one append-only log to copy:
`Documents/Investigations/Engine/RefactorCandidates.md`. Its header owns its
row rules, main appends rows itself without reading code, and two sites exempt
a *modified* copy of it from review: the coherence-trigger condition in
`.agents/scripts/Get-SessionChangeInventory.ps1` (the `foreach ($entry in
$Entries)` loop, the condition after the comment "A modified
refactor-candidate log never fires it either") and the paragraph beginning "A
modified `Documents/Investigations/Engine/RefactorCandidates.md`" in
`.agents/skills/finalize-changes/references/landing-acceptance-table.md`
`## Acceptance table`. `Documents/Investigations/` is never a scheduler input
(`Documents/AGENTS.md` `## Planning Trees`), and its `ChangeWorkflow/`
subject folder already exists.

## Design

The author recommends one new log, owned by its own header the way
`RefactorCandidates.md` is, with the routing sites cut down to point at it.

1. Location: `Documents/Investigations/ChangeWorkflow/WorkflowIncidents.md`.
2. Row format: one Markdown table row per occurrence, under `## Entries`:
   `| <owner> | <symptom> | <YYYY-MM-DD> | <session branch> | <evidence> |`.
   A table row is one line whatever its length (the static-checks
   `line-length` row skips table rows), so one Grep of the owner returns
   every row of that owner whole, and counting needs no script.
3. Similarity key: owner plus symptom. The owner is the script that
   misbehaved, or the skill directory (`.agents/skills/<name>`) or the
   `.agents/references/` file whose text was at fault — main derives it from
   the finding's `emitter:` field. The symptom is a short phrase naming the
   problem; two occurrences share it when one fix would remove both. The
   recorder reuses an existing row's symptom word for word when it matches, so
   matching rows stay identical in their first two columns.
4. Who appends and when: main, reading no further code, from the accepted
   finding — at the `/next-plan` checkpoint, when it observes friction after
   that checkpoint, and when it accepts a workflow-friction or
   token-efficiency finding of `/next-plan-review`. The row joins the
   session's landing set like any follow-up content.
5. Third occurrence: when the new row is the third with its key, main
   dispatches one `implementer` for `/create-follow-up-plans` with the key's
   rows as evidence plus the `Session provenance` input. That worker creates,
   maps, or rejects the Plan as today and deletes the key's rows in the same
   change; the Plan restates the rows, since the log loses them. The user chose
   this route (`## Context`): always a Plan at the threshold, never a fix
   inside the current change. It also works from a `/next-plan-review` session
   with no change of its own and keeps workflow edits out of an unrelated
   Plan's change. This rule governs only log rows; a finding item 6 exempts
   never enters the log.
6. Exemption, the user's decision (`## Context`): an objectively wrong
   statement or a bug is never recorded. Such a finding points to one statement
   or result and shows it false: instruction text that states something untrue
   or contradicts the file that owns the fact, or a script or skill whose
   output, tracked content, or pass or refuse verdict contradicts its
   documented contract. A finding that shows only a cost — extra steps,
   retries, a workaround, context spent, or wording that is unclear but true —
   is friction and is recorded. An exempt finding follows the Change Workflow
   Leftovers rule unchanged, as every accepted finding does today: fixed inside
   the current change when it is small, otherwise sent to
   `/create-follow-up-plans` as an ordinary proposal. The author recommends
   that rule as the reading of the user's "should always be fixed": it already
   fixes a small finding at once and plans the rest, with no new text. An
   `active-change-blocker` finding stays as today: it returns to the current
   change and is never recorded.
7. Unaffected: engine, game, and tool defects and every other Change Workflow
   leftover keep the Leftovers rule; `RefactorCandidates.md` and
   `/refactor-candidates` are unchanged.
8. Prose, not a script. The author recommends prose because the match is a
   judgment ("one fix would remove both") that a script cannot make, and the
   count is one Grep of single-line rows; a script would add a file, a
   documented invocation, and a result contract, more text than it removes.

Proposed header for the new file (the implementer may tighten wording, not
meaning):

```markdown
# Workflow Incidents

One row per routed friction, context-efficiency, or isolation finding. Not
recorded: an objectively wrong statement or bug — instruction text that is
untrue or contradicts its owner, or a script or skill breaking its documented
contract. Cost-only findings, even unclear but true wording, are recorded.

Key: owner (script, skill, or reference file at fault) plus symptom (rows share
it when one fix removes both). Main, reading no code, adds its row first,
reusing a matching symptom verbatim. Evidence: selector, result, cost or
bounding mechanism; `|` as `;`; no transcript text or path, no absolute path.

At a key's third row, never sooner, main has one `implementer` run
`/create-follow-up-plans` on its rows, deleting them once the Plan is created,
mapped, or rejected.

## Entries

| Owner | Symptom | Date | Session branch | Evidence |
| --- | --- | --- | --- | --- |
```

Match the new file's encoding and line endings to `RefactorCandidates.md`
(UTF-8 without BOM, final newline).

## Critical files

- `Documents/Investigations/ChangeWorkflow/WorkflowIncidents.md` (new)
- `.agents/skills/next-plan/references/run-checkpoint.md`
- `.agents/skills/create-follow-up-plans/SKILL.md`
- `.agents/skills/create-follow-up-plans/references/worker.md`
- `.agents/skills/create-follow-up-plans/references/tooling-friction-plan-template.md`
- `.agents/skills/next-plan-review/SKILL.md`
- `.agents/skills/finalize-changes/references/landing-acceptance-table.md`
- `.agents/scripts/Get-SessionChangeInventory.ps1`

## In scope

- Create `Documents/Investigations/ChangeWorkflow/WorkflowIncidents.md` with
  the header above and an empty `## Entries` table.
- `run-checkpoint.md` `## Follow-up routing`: replace the first paragraph
  ("Each accepted finding follows the Leftovers rule ...") with: main records
  each accepted finding in the log as its header directs, and a finding that
  header exempts keeps the Leftovers rule; the threshold `implementer` routes a
  key to `/create-follow-up-plans` as a tooling-friction or isolation proposal
  with the provenance block sourced per `follow-up-provenance.md`; an
  `active-change-blocker` finding is never recorded and returns to the current
  change as a blocker. In the third paragraph, friction first observed after
  the checkpoint "is handled the same way" instead of "is recorded through
  `/create-follow-up-plans` by an `implementer`", and
  "the checkpoint's Plans" becomes "the checkpoint's rows and Plans". The
  middle paragraph is unchanged.
- `run-checkpoint.md` `## Measurement states`: in the table's
  `Handoff line recorded` cells of the `pass`, `needs-review`, blocked or
  error, and `breachRowsTruncated` rows, replace each Plan-path placeholder
  with `<routing result>`, and in the paragraph after the table ("The
  `Handoff line recorded` column is main's own record ...") define it once:
  the Plan path(s) the routing produced, `logged <n>` for rows recorded, or
  `none`.
- `create-follow-up-plans/SKILL.md`: the frontmatter `description` clause
  "and for tooling-friction follow-ups recorded at a /next-plan claim exit"
  becomes "and for a tooling-friction or context-efficiency problem once the
  workflow incident log holds three rows of it"; the `## When to use` bullet
  "A tooling-friction follow-up is recorded at a `/next-plan` claim exit."
  becomes a bullet naming the log path and its three-row threshold; delete the
  sentence "An evidence-free proposal — ... — stays rejected for a
  tooling-friction or context-efficiency follow-up too." (the
  evidence-free rejection before it already covers it).
- `create-follow-up-plans/references/worker.md` `## Rules`:
  - Replace the two bullets "Tooling friction observed during a `/next-plan`
    run is a separate category." and "So is a script or documented tool
    invocation ..." with one bullet: tooling-friction and context-efficiency
    proposals are a separate category, each arriving with the rows of one key
    of the log, which this worker deletes as that file's header directs.
  - In the substitution bullet, replace the two sentences from "For a
    tooling-friction proposal, that evidence is the in-session citation"
    through "the rework or workaround it forced." with one sentence: for a
    tooling-friction or context-efficiency proposal, that evidence is its
    incident rows — owner, symptom, and each row's evidence — which also
    substitute for the unmet acceptance criterion or accepted residual.
  - In the following bullet, "the grouping key is (skill or script, observed
    symptom)" becomes "the grouping key is its incident key".
- `tooling-friction-plan-template.md`: the `## Context` placeholder line
  becomes one line per incident row (date, session branch, selector, observed
  output or behavior, what was repeated or worked around), still with no
  transcript text. The provenance block is unchanged and names the session
  that recorded the third row.
- `next-plan-review/SKILL.md` `## Handoff`: extend the sentence "Main presents
  the retrospective, decides each returned finding, and owns every follow-up
  action." with: a workflow-friction or token-efficiency finding main accepts
  is recorded in the log as its header directs.
- `landing-acceptance-table.md` `## Acceptance table`: extend the
  `RefactorCandidates.md` paragraph to the new log — a modified copy owes no
  Review and resolve correctness row; rows main records under the log's header
  are authorized by that header, and the `/create-follow-up-plans` worker's
  row deletions by that skill.
- `Get-SessionChangeInventory.ps1`: in the `foreach ($entry in $Entries)`
  coherence condition, exempt status `M` of
  `Documents/Investigations/ChangeWorkflow/WorkflowIncidents.md` exactly as
  `RefactorCandidates.md` is exempted (an inline list of the two paths is
  enough), and extend the comment above it to name both logs.

## Out of scope

- The session-provenance machinery: the provenance block in
  `tooling-friction-plan-template.md`, `follow-up-provenance.md`, and
  `/next-plan-review` bounded friction mode stay as they are; removing it is
  a possible separate Plan, not this one.
- `/next-plan-checkpoint-review` and its finding forms, lenses, and classes;
  `change-workflow.md`, including the Leftovers rule and Step 9; `/next-plan`
  `SKILL.md`, including its post-checkpoint outcome table (a log row is
  already "tracked follow-up content" there).
- Engine, game, and tool defects and every other Change Workflow leftover;
  `RefactorCandidates.md` and `/refactor-candidates`.
- Seeding the log, row expiry, or a reviewing skill for the log; converting
  existing Plans into rows.
- Any script that appends or counts rows.
- `Projects/Coretide/` content of any kind.

## Risk tier and invariants

Expected Tier 2. Trigger: scoped tool behavior — the
`Get-SessionChangeInventory.ps1` coherence trigger changes for one path. The
rest is instruction text.

Invariants:

- The inventory exemption is keyed on status `M` and the exact path, so
  creating or deleting the log still fires the coherence review, as for
  `RefactorCandidates.md`.
- No log row carries a transcript path, transcript text, a home or absolute
  path, or Coretide content (this repository is public).
- No route sends a single tooling-friction, context-efficiency, or isolation
  observation to `/create-follow-up-plans` or to an in-change fix, except a
  finding the log's header exempts as an objectively wrong statement or a bug.
- No backward compatibility: no text keeps the old single-observation route.

## Acceptance criteria

- `Documents/Investigations/ChangeWorkflow/WorkflowIncidents.md` exists with
  the header and an `## Entries` table with no rows.
- A Grep of `.agents` for `claim exit` finds no intake of tooling-friction
  follow-ups in `create-follow-up-plans/SKILL.md`, and a Grep for
  `Each accepted finding follows the Leftovers rule` finds no match in
  `run-checkpoint.md`.
- Every remaining site naming tooling-friction, context-efficiency, or
  isolation follow-ups (`run-checkpoint.md`, `create-follow-up-plans`
  `SKILL.md` and `references/worker.md`, `next-plan-review/SKILL.md`) routes a
  finding through the log or its threshold.
- Against a baseline commit that contains the log, running
  `pwsh -NoProfile -File .agents/scripts/Get-SessionChangeInventory.ps1
  -RepositoryRoot '<worktree root>' -Baseline <that SHA>` with only a
  working-tree edit adding one row to the log reports `coherenceReview` false;
  the same run with an added `Documents/Investigations/ChangeWorkflow/` file
  instead reports true. Both edits are reverted afterward.
- The static-checks runner, invoked as `.agents/references/change-workflow.md`
  `#### Step 5 — Run targeted pre-review checks` documents it, reports every
  row the change triggers passing.
- The edited existing files together lose words (measured with `wc -w` against
  the baseline), per `## Notes`.

## Notes

Expected net word change, measured on the touched files: the seven edited
existing files lose about 55 words in total (`worker.md` about -70,
`create-follow-up-plans/SKILL.md` about -15, `run-checkpoint.md` about -10;
`next-plan-review/SKILL.md` and `landing-acceptance-table.md` about +20 each;
the template about 0; the script change is code plus a one-line comment). The
new log file adds about 150 words, so the whole change is about +95 words
net. Every rule the log introduces lives in its header; the routing sites only
point to it.
