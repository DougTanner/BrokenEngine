<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-24T00:57:33.829Z","dependsOn":[]} -->
# Fix: .agents/references/subagent-reporting.md — cite saved findings by path

## Context

Three sessions triggered the
`Documents/Investigations/ChangeWorkflow/WorkflowIncidents.md` key (owner:
`.agents/references/subagent-reporting.md`; symptom: main repeats saved review
findings inline in worker briefs instead of citing output path and finding IDs).
`.agents/references/subagent-reporting.md:94-97` (`## Task brief`) already
directs main to cite readable review outputs by path/selector plus finding IDs
(with manager decisions inline) rather than pasting or paraphrasing findings,
yet main repeated saved review findings inline across three independent runs:

- 2026-11-23, `agy/d44a0c7b-fc2b-4cfa-823e-087ffabfac57`: audit, coherence, and
  validation worker briefs repeated saved review findings inline across multiple
  briefs instead of passing saved output paths and accepted finding IDs with
  manager decisions inline.
- 2026-11-23, `agy/fb20f552-143b-4738-8e30-6ae3d293bfa6`: planner write-in and
  `/resolve-findings` briefs repeated accepted findings inline alongside saved
  review paths instead of passing saved review paths and accepted finding IDs
  with manager decisions only.
- 2026-11-23, `agy/0dd9242c-5f71-4111-812c-71056a74cb82`: `/resolve-findings`
  brief repeated the `Engine/Source/Ui/AGENTS.md:147` progressive-disclosure
  finding inline alongside the saved review output path instead of citing the
  saved output path and finding locator with decision and prescribed check
  inline.

Session provenance (machine-local; not reproducible after cleanup). The Client
through Worktree fields name the session that observed the friction — the
session `/next-plan-review` must reach — while the `Landing ref` line names a
ref whose tree actually contains this Plan:

- Client: agy
- Conversation session ID: a8e4e353-ba69-4ad6-8bde-e7cb17f5b4e7
- Worktree/branch UUID: 0dd9242c-5f71-4111-812c-71056a74cb82
- Session branch: agy/0dd9242c-5f71-4111-812c-71056a74cb82
- Worktree: .agy\worktrees\BrokenEngine\0dd9242c-5f71-4111-812c-71056a74cb82
- Landing ref: agy/0dd9242c-5f71-4111-812c-71056a74cb82.
  The review ref the invoker passes to `/next-plan-review` has two cases. When
  the `Observed in an earlier session` line is present, it is that line's
  landed commit hash, with no lookup. When that line is absent, the recording
  session is the observing session, and worktree reuse moves its branch past
  its landing, so the invoker never reviews the ref itself: it resolves the ref
  to the commit that added this Plan with the command below and passes that
  commit:

  ```powershell
  git log --follow --diff-filter=A -n 1 --format=%H agy/0dd9242c-5f71-4111-812c-71056a74cb82 -- Documents/Plans/ChangeWorkflow/CiteSavedFindingsByPathInWorkerBriefs.md
  ```

  When the recorded ref is gone or that command prints nothing (a later session
  moved this Plan), run the same command with the ref omitted. A periodic
  Plan-history squash can make either command return an unrelated aggregate
  commit, and a landing that also deletes a similar file can make it return
  another session's commit, so review the result only when the commit is
  attributable to the observing session recorded above alone (its diff limited
  to that session's files); never review an aggregate or multi-session squash
  commit.
- Run the review before `/cleanup-worktrees` removes the worktree recorded
  above: Codex discovery requires a registered worktree, and Claude or agy
  review requires the conversation session ID above.

## Design

First root-cause the friction from the current tree and this Plan's
`## Context`. Only when the transcript is genuinely needed, in a new session run
`/next-plan-review <review ref>` in bounded friction mode, with the review ref
the provenance block above defines, supplying the recorded client and the
recorded conversation session ID. Inspect why main still repeats saved findings
inline despite `.agents/references/subagent-reporting.md:94-97` — for example:
whether `.agents/references/subagent-reporting.md:28-41` (`## Task brief`
template) lacks an explicit finding-reference slot while burying the citation
rule below, whether session-start context in
`.agents/references/change-workflow.md` Steps 3 and 6 omits the path-plus-ID
form on write-in, `/resolve-findings`, and re-review dispatches, and whether
skill `## Inputs` wording (such as `.agents/skills/resolve-findings/SKILL.md:31`
and `.agents/skills/coherence-review/SKILL.md:45-54`) pulls main toward
restating findings inline.

Then make the smallest fix inside the `## In scope` boundary below, honoring the
root `AGENTS.md` progressive-disclosure and instruction-changes directives (keep
each rule once at its owning layer and tighten existing prose rather than
duplicating it). If root-causing shows the fix lies outside that boundary (for
example, a host hook or script change), surface it for re-planning instead of
expanding scope.

## Critical files

- `.agents/references/subagent-reporting.md` (`## Task brief`, lines 24-104) —
  incident owner declaring the task-brief form and saved-output citation rule.
- `.agents/references/change-workflow.md` (`#### Step 3 — Plan review`,
  lines 245-249; `#### Step 6 — Review and resolve correctness`,
  lines 331-337) — session-start workflow context for write-in,
  `/resolve-findings`, and re-review dispatches.
- `.agents/references/subagent-handoff.md` (`## Continuation capsule`,
  lines 78-93) — continuation-capsule artifact-citation form used on write-in
  briefs.
- `.agents/skills/resolve-findings/SKILL.md` (`## Inputs`, lines 25-40) —
  accepted-finding input wording.
- `.agents/skills/coherence-review/SKILL.md` (`## Inputs`, lines 40-55) —
  re-review `First-round findings` input wording.
- `.agents/skills/prepare-change/SKILL.md` (`## Inputs`, lines 31-65) —
  planner write-in input wording.

## In scope

- Root-cause investigation as `## Design` states.
- The smallest resulting fix, confined to the sections listed in
  `## Critical files` above.

## Out of scope

- The landed change the observing session produced (lighting combine curve
  sampling).
- Unrelated skills, scripts, hooks, C++ or shader code, and any transcript path
  or transcript text in the repository.

## Risk tier and invariants

Expected Tier 2 (scoped workflow/tool behavior; trigger: Change Workflow
brief-authoring and dispatch instructions across `.agents/references/` and
`.agents/skills/`); escalate to Tier 3 if the fix reaches build/bootstrap
coordination. Never embed transcript paths or home paths. No determinism/CRC,
serialization, replay, wire, affinity, threading, allocation, shader, or build
exposure.

## Acceptance criteria

- The recorded symptom no longer reproduces under the documented Change Workflow
  dispatches: planner write-in, `/resolve-findings`, and re-review briefs cite
  saved review output paths plus finding IDs/locators with inline manager
  decisions instead of repeating finding bodies inline.
- The static-checks runner, invoked as `.agents/references/change-workflow.md`
  `#### Step 5 — Run targeted pre-review checks` documents it, reports every
  row the change triggers passing.

## Notes

- No `/agent-harness` run or MSBuild target applies; verify with the
  static-checks runner and checkpoint inspection of dispatched worker briefs.
