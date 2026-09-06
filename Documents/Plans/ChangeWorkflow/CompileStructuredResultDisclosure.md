<!-- broken-engine-plan/v1 {"createdUtc":"2026-09-06T17:31:27.889Z","dependsOn":[]} -->
# Move `/compile`'s build-result schema list out of the public `SKILL.md` and state its authority rule once

## Context

`.agents/skills/compile/SKILL.md` carries a `### Structured build result`
section between `## Handoff` and `## References`. Two progressive-disclosure
problems in it were proven from the current tree by a
`/progressive-disclosure-review` reviewer and confirmed by reading the file.

Duplicated rule. The same instruction is stated twice in the same file:

- `.agents/skills/compile/SKILL.md:63`, inside `## Handoff`: "Read every
  reported field from the envelope, never from scraped terminal text."
- `.agents/skills/compile/SKILL.md:95`, inside `### Structured build result`:
  "The JSON — not scraped terminal text — is the authoritative result."

Misplaced schema. `.agents/skills/compile/SKILL.md:91-104` lists the whole
`broken-engine-build-result/v1` field set — `status`, `failureKind`, `exitCode`,
`target`/`worktreeRoot`, `arguments`, `selectedFiles`, `invalidatedObjects`,
`lock`, `msbuild`, `retainedLog`, `diagnostics`, `messages`,
`elapsedMilliseconds`, `startedAt` — and tells its reader to "Capture stdout,
parse it, and read" them. That is worker mechanics and a schema field list. The
root `AGENTS.md` progressive-disclosure directive places mechanics and schemas
in scripts and skill `references/`, not in the public file, and
`.agents/references/skill-skeleton.md` `## Section order` lists no such section
for `SKILL.md`: its `## Public and private files` rule keeps `SKILL.md` to what
main needs to dispatch and to deliver verbatim, and the parse-and-read
instruction is addressed to the dispatched worker, which reads
`.agents/skills/compile/references/worker.md`.

The worker file already owns the neighbouring mechanics. Its step 10
(`.agents/skills/compile/references/worker.md:182-200`) tells the worker to read
the outcome from the schema name on stdout and distinguishes
`broken-engine-build-result/v1` from `broken-engine-compile-invoke-result/v1`;
its step 11 (`:203-218`) owns capturing each envelope into the dispatch's
`Temp/AgentBuildEnvelopes/` file. The field list is the same topic, one file
away from where it belongs.

Both regions were outside the approved boundary of the change this Plan was
authored alongside: that change's `## Out of scope` named
`### Structured build result` and the frontmatter of the same file explicitly,
and its diff touched only one bullet of `## Handoff`. Nothing here is a
regression from it; both conditions predate it.

## Design

Recommended approach: keep the authority rule in exactly one place, and move the
schema field list to the worker file that already owns the surrounding
mechanics.

1. Move the field list at `.agents/skills/compile/SKILL.md:96-104` into
   `.agents/skills/compile/references/worker.md`, next to the step that already
   classifies the stdout envelope (currently step 10). The author recommends the
   worker file over a new `references/` file: the list is about ten lines, the
   worker step it serves is adjacent, and a new file would add a hop without
   adding an owner. Prefer a new reference only if placing the list inline makes
   the step's own paragraph exceed the 4-line limit
   `.agents/references/skill-skeleton.md` `## Section order` sets for a worker
   step — in which case link it from `references/worker.md`, not from
   `SKILL.md`, since only the worker reads it.
2. Delete the duplicate authority sentence. The author recommends keeping
   `.agents/skills/compile/SKILL.md:63` — it sits with the envelope-file bullet
   main and the worker both act on — and dropping the restatement at
   `.agents/skills/compile/SKILL.md:95`, carrying no copy of it into the worker
   file, because step 10 there already tells the worker to read the outcome from
   stdout's schema rather than from anything scraped.
3. Remove the now-empty `### Structured build result` heading and its remaining
   lead-in from `SKILL.md`, rather than leaving a stub section. The one fact in
   that lead-in main might still need — that `WorktreeCli build` writes exactly
   one `broken-engine-build-result/v1` object to stdout while human progress
   goes to stderr — is already implied by the `## Handoff` envelope bullet; if a
   reviewer judges it load-bearing for main, keep it as a clause on that
   existing bullet rather than as a section.

Leave the `## Handoff` field bullets (`SKILL.md:64-77`) untouched. They state
what the handoff reports, not what the schema contains, and they are the reason
main needs no schema list.

If root-causing shows a consumer outside `.agents/skills/compile/` reads the
schema list at its current path, surface that for re-planning instead of
expanding scope.

## Critical files

- `.agents/skills/compile/SKILL.md` — `### Structured build result` (`:91-104`),
  the section to empty and remove, and the duplicate sentence at `:95`; plus the
  retained sentence at `:63`.
- `.agents/skills/compile/references/worker.md` — steps 10 and 11 (`:182-218`),
  the destination for the field list.
- `.agents/references/skill-skeleton.md` — read-only: `## Section order` and
  `## Public and private files`, the rules that place the section.
- `AGENTS.md` — read-only: the progressive-disclosure directive.

## In scope

- `.agents/skills/compile/SKILL.md` `### Structured build result` (`:91-104`):
  remove the duplicated authority sentence and relocate the
  `broken-engine-build-result/v1` field list, then remove the emptied section as
  `## Design` step 3 states.
- `.agents/skills/compile/references/worker.md`: receive the field list beside
  the existing stdout-classification step, or a new
  `.agents/skills/compile/references/` file linked from `references/worker.md`
  when `## Design` step 1's length condition holds.
- The retained authority sentence at `.agents/skills/compile/SKILL.md:63`, only
  if `## Design` step 3 keeps a stdout/stderr clause on that bullet.
- `/validate-skill` on the `compile` skill package and
  `/progressive-disclosure-review` on the changed files.

## Out of scope

- The `broken-engine-build-result/v1` schema itself: its field names, its
  values, its truncation behavior, and everything WorktreeCli emits. This change
  moves text; it changes no emitted byte.
- The `## Handoff` field bullets at `.agents/skills/compile/SKILL.md:64-77` and
  the `Decisive checks`, `Evidence`, and `Residuals` narrowings that follow
  them — what `/compile` reports to main does not change here.
- `## Purpose`, `## When to use`, `## Inputs`, `## References`, and the
  frontmatter of `.agents/skills/compile/SKILL.md`.
- Every `.agents/skills/compile/scripts/**` file,
  `.agents/skills/compile/references/prefast-mode.md`, and
  `.agents/skills/compile/references/runtime-data-mode.md`.
- `.agents/references/subagent-reporting.md`, `.agents/references/skill-skeleton.md`,
  the root `AGENTS.md`, and every other skill package.
- Any C++, GLSL, or project-membership change.

## Risk tier and invariants

Expected Tier 1 under the root `AGENTS.md` risk tiers; this author's
classification, to be confirmed at the Approve and classify step. The trigger is
mechanical documentation work: relocating existing prose between two files of
one skill package and deleting one duplicate sentence, with no public signature
and no invariant exposed. Escalate to Tier 2 if the move turns out to change
what the `builder` reports or how it classifies a build outcome.

Invariants to preserve:

- Every fact the two regions state today survives somewhere in the package: no
  schema field is dropped, and the authority rule still reaches the worker.
- `SKILL.md` alone still suffices for main to dispatch `/compile` and to read
  its handoff, per `.agents/references/skill-skeleton.md`
  `## Public and private files`.
- The worker still classifies stdout by schema name and still writes each
  envelope verbatim to the dispatch's `Temp/AgentBuildEnvelopes/` file.
- Changed files keep their existing encoding and line endings.

## Acceptance criteria

- `.agents/skills/compile/SKILL.md` states the "read from the envelope, not from
  scraped terminal text" rule exactly once.
- The `broken-engine-build-result/v1` field list no longer appears in
  `.agents/skills/compile/SKILL.md`, and every field it listed appears once
  under `.agents/skills/compile/references/`.
- `.agents/skills/compile/SKILL.md` has no section outside the order
  `.agents/references/skill-skeleton.md` `## Section order` defines.
- `/validate-skill` reports the `compile` package valid, and
  `/progressive-disclosure-review` raises no duplication or misplacement finding
  against the changed files.
- `pwsh -NoProfile -File .agents/scripts/Test-PlanSchedulerState.ps1` reports
  `status: valid` and `code: ok`.

## Notes

Another live Plan edits `.agents/skills/compile/SKILL.md` `## Handoff` on a
different axis and names `### Structured build result` in its own
`## Out of scope`, so the two boundaries are disjoint and neither blocks the
other. This Plan touches `## Handoff` only in the single optional case
`## Design` step 3 describes; check that section's current wording before
editing it.
