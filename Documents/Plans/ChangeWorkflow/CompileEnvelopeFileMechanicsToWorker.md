<!-- broken-engine-plan/v1 {"createdUtc":"2026-09-06T14:44:19.340Z","dependsOn":[]} -->
# Move the compile envelope-file mechanics into the worker reference

## Context

A progressive-disclosure review found that the `## Handoff` section of
`.agents/skills/compile/SKILL.md` — the public file — now states the envelope
file's storage mechanics: that every build's captured
`broken-engine-build-result/v1` envelope is written verbatim into one
`Temp/AgentBuildEnvelopes/` Markdown file unique to the dispatch, under a single
`##` heading with one fenced block per build, with the first build's own
PowerShell call creating the file and heading and each later build's call
appending its block.

The root `AGENTS.md` Public/private skills directive puts that class of content
in the private file: a `SKILL.md` carries only what a parent session needs to
decide on and dispatch the skill, while
`.agents/skills/compile/references/worker.md` carries the steps and rules the
dispatched worker follows. The owning layer for these mechanics is
therefore `.agents/skills/compile/references/worker.md` step 11 (see
`.agents/skills/compile/references/worker.md:203-205`, "Parse and report only the
structured result described above after every requested target has returned").

The change that introduced the envelope file could not do the move itself: its
Plan lists `.agents/skills/compile/references/**`, including the worker's
execution and result discipline, in `## Out of scope`.

This is a layering leftover, not a defect. A measured build in the session that
introduced the text proved a dispatched `builder` executes the contract
correctly from the public file alone, so nothing is broken today; only the
placement is wrong.

Line numbers inside `.agents/skills/compile/SKILL.md` may have shifted since this
Plan was written, because that file was being trimmed concurrently. Locate the
text by its `## Handoff` heading and by the quoted wording above rather than by
line number.

## Design

Recommended approach: move the how into the worker reference and leave the
public `## Handoff` with the citing contract only. The mechanism itself was
settled by the originating session and this Plan does not reopen it; the work
relocates the layout and the create-then-append mechanics, adds the concrete
file-name recipe the public file only gestured at, and trims what the move
allows.

1. Add the mechanics to `.agents/skills/compile/references/worker.md` step 11,
   as sub-bullets of that step, since step 11 is where the worker parses and
   reports the structured result:

   - Every build's captured `broken-engine-build-result/v1` envelope is written
     verbatim into one `Temp/AgentBuildEnvelopes/` Markdown file unique to the
     dispatch.
   - The `Temp/AgentBuildEnvelopes/` directory is created beforehand if absent,
     as a step of its own. `Temp/` is gitignored, so the directory is absent in
     every fresh worktree, and the root `AGENTS.md` bundled-scripts rule allows
     nothing chained before or after the build's own script call.
   - The file name is `compile-<UTC yyyyMMddTHHmmssfffZ>-<process id>.md`. This
     mirrors the retained build log's existing naming precedent at
     `Tools/WorktreeCli/BuildCommand.cpp:107-109`, which composes a base name
     from the lowercased target stem, a UTC `yyyyMMddTHHmmssfffZ` timestamp, and
     the current process id; the recipe here keeps the timestamp and process id
     and uses a fixed `compile-` prefix in place of the target stem, because one
     envelope file covers a whole dispatch rather than a single target. The
     rationale for reusing that recipe is that it is already proven unique
     enough for concurrent build writers in this repository, so no new
     uniqueness scheme has to be justified.
   - The file carries a single `##` heading for the dispatch, with one fenced
     block per build.
   - The first build's own PowerShell call creates the file and the heading, and
     each later build's own call appends its block to the same file. Each block
     is written from that call's own captured output — never a new script, never
     a retyped envelope. This is consistent with the root `AGENTS.md`
     bundled-scripts rule, which allows using a script call's own output from
     the PowerShell tool in that same call.

2. Trim `.agents/skills/compile/SKILL.md` `## Handoff` so it keeps only the
   citing contract a parent session needs in order to read the results:

   - Every build's envelope is recorded verbatim in this dispatch's envelope
     file under `Temp/AgentBuildEnvelopes/`.
   - A clean-success build — `status: success`, `failureKind: none`,
     `exitCode: 0`, `retainedLog.complete: true`, and no `severity: error`
     diagnostic — is cited by that file instead of being carried inline; every
     other build's envelope is also included inline.
   - `Evidence` names this dispatch's envelope file as path plus selector.

   The layout, the directory-creation precondition, the create-then-append
   sequence, and the naming recipe leave the public file.

3. Change nothing about what the handoff reports, and nothing about the envelope
   file's location, shape, or contents. The only change to what the builder does
   is that the file's name, previously unstated, is now fixed by the recipe in
   step 1.

## Critical files

- `.agents/skills/compile/SKILL.md` — `## Handoff`; the source of the misplaced
  mechanics.
- `.agents/skills/compile/references/worker.md` — step 11 at
  `.agents/skills/compile/references/worker.md:203-205`; the owning layer.
- `.agents/references/subagent-reporting.md` — the shared handoff form the
  compile handoff extends; read-only reference for this change.
- `Tools/WorktreeCli/BuildCommand.cpp` — lines 107-109; read-only precedent for
  the unique-name recipe.

## In scope

- The `## Handoff` section of `.agents/skills/compile/SKILL.md`: remove the
  envelope-file layout, the directory-creation precondition, the create-and-append
  sequence, and any naming detail, and keep the citing contract listed in
  `## Design` step 2.
- Step 11 of `.agents/skills/compile/references/worker.md`: add the envelope-file
  mechanics listed in `## Design` step 1, including the directory-creation
  precondition and the concrete name recipe.
- `/validate-skill` and `/progressive-disclosure-review` on the `compile` skill
  package after the edits.

## Out of scope

- The envelope mechanism itself: whether an envelope file exists, its
  `Temp/AgentBuildEnvelopes/` location, the one-file-per-dispatch shape, the
  create-then-append sequence, and the clean-success citing rule are settled and
  are only relocated here.
- The `broken-engine-build-result/v1` schema, its fields, and its truncation
  behavior.
- `.agents/skills/compile/scripts/**`,
  `.agents/skills/compile/references/runtime-data-mode.md`, and
  `.agents/skills/compile/references/prefast-mode.md`.
- `## When to use`, `## Inputs`, `## Purpose`, `### Structured build result`, and
  the frontmatter of `.agents/skills/compile/SKILL.md`.
- The shared handoff form, its row set, and its 40-line/20,000-character caps in
  `.agents/references/subagent-reporting.md`.
- Adding any script, and any change to `Tools/WorktreeCli/`.
- Any other skill package.

## Risk tier and invariants

Tier 1. Trigger: documentation-only layering work with no public signature or
invariant exposure — the change moves prose between two Markdown files in one
skill package and leaves the envelope file's location, shape, and contents and
the reported handoff fields identical. It fixes the envelope file's name, which
no document states today, and that name is an ignored `Temp/` scratch path no
other document or script reads.

Escalate to Tier 2 if the reviewer judges that fixing the name recipe, or
editing the worker step, changes the dispatched builder's behavior rather than
restating it; the Tier 2 trigger would then be one subsystem's tool behavior,
confined to the `compile` skill.

Invariants to preserve:

- The public/private skill split in the root `AGENTS.md`: `SKILL.md` states
  purpose, triggers, inputs, and handoff;
  `.agents/skills/compile/references/worker.md` states steps and rules.
- Progressive disclosure: each fact lives once at its owning layer, so the
  mechanics must not remain duplicated in both files.
- No new script, and no retyped envelope: the envelope block is written from the
  build call's own captured output.

## Acceptance criteria

- `.agents/skills/compile/SKILL.md` `## Handoff` no longer states the envelope
  file's layout, heading and fenced-block structure, directory-creation
  precondition, create-or-append sequence, or name recipe, and still states that every build's envelope goes to this
  dispatch's envelope file, that a clean-success build is cited by that file
  while every other build's envelope is also inline, and that `Evidence` names
  the file as path plus selector.
- `.agents/skills/compile/references/worker.md` step 11 states all of the
  mechanics in `## Design` step 1, including the directory-creation
  precondition and the name `compile-<UTC yyyyMMddTHHmmssfffZ>-<process id>.md`.
- No fact from `## Design` step 1 appears in both files.
- `/validate-skill` reports the `compile` package valid.
- `/progressive-disclosure-review` raises no finding of this class against the
  `compile` package.

## Coordination

`Documents/Plans/ChangeWorkflow/SkillHandoffMinimumReturnAudit.md` audits every
skill's `## Handoff` against the minimum-return principle and may itself edit the
`## Handoff` section of `.agents/skills/compile/SKILL.md`. The two changes are
not directional — either may run first — but they touch the same region, so
whichever runs second re-reads that section as it then stands instead of assuming
the wording quoted here.

## Notes

The originating session's own Plan could not absorb this work because it excluded
`.agents/skills/compile/references/**`.
