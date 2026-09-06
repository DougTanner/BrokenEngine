<!-- broken-engine-plan/v1 {"createdUtc":"2026-09-06T16:05:40.704Z","dependsOn":["Documents/Plans/ChangeWorkflow/CompileEnvelopeFileMechanicsToWorker.md"]} -->
# Drop `/compile`'s inline-verbatim envelope clause and narrow the typed-envelope cap exemption

## Context

A tree-wide audit of every skill's `## Handoff` against the minimum-return
principle in `.agents/references/subagent-reporting.md` `## Handoffs` proved one
violation in `/compile` that the auditing change could not fix: its approved
boundary put `/compile`'s envelope reporting with a separate change, so
`.agents/skills/compile/SKILL.md` `## Handoff` and
`.agents/skills/compile/references/**` were outside every fix it was allowed to
make.

The proven violation, from the current tree:

- `.agents/skills/compile/SKILL.md:69-72` requires the `builder` to "Include a
  build's envelope verbatim inline as well, unless that build is a clean
  success". For every build that is not a clean success, the whole
  `broken-engine-build-result/v1` envelope therefore enters the main session as
  text.
- The same bytes are already written verbatim to this dispatch's
  `Temp/AgentBuildEnvelopes/` file by `.agents/skills/compile/SKILL.md:61-68`,
  and that file is already cited under `Evidence` as path plus selector by
  `.agents/skills/compile/SKILL.md:93-94`.
- The consumer that actually requires the whole envelope verbatim is the
  `/finalize-changes` worker, not main:
  `.agents/skills/finalize-changes/references/landing-acceptance-table.md:67-72`
  requires the authoritative envelope for the build row, and
  `.agents/skills/finalize-changes/references/worker.md:76-78` requires
  consuming each envelope verbatim, read from the cited `Temp/` path plus
  selector.
- Main's own decisions are already settled by the fields
  `.agents/skills/compile/SKILL.md:73-86` mandates separately: per-project
  `status`, `exitCode`, and `failureKind`; every `severity: error` diagnostic's
  `raw` line and all `messages`; changed-file warning `raw` lines; each
  `retainedLog.path` and its `complete` value; and, for game builds, the
  data-mode fields. The remaining envelope members — `arguments`,
  `selectedFiles`, `invalidatedObjects`, `lock`, `msbuild` discovery state,
  `worktreeRoot`, `elapsedMilliseconds`, `startedAt` — are bytes main never
  decides on.

The coupled leftover is the typed-envelope exemption sentence at
`.agents/references/subagent-reporting.md:120-123`, which exempts the
`broken-engine-build-result/v1` envelope and the typed receipts
`/finalize-changes` consumes from the 10-row/40-line/20,000-character caps. The
audit did not prove that sentence false — each exemption names a real consumer
that requires verbatim bytes — but it did prove that both exemptions are written
over the whole artifact while their consumer needs only some of it, and that both
artifacts are already file-routed at a cited path plus selector
(`.agents/skills/finalize-changes/references/scripts.md:5-24` for the receipts).
The build-envelope half of that sentence can only be narrowed after the inline
clause above is gone, because while the clause stands the exemption is what makes
the inline envelope legal; the two edits are one unit of work.

Commit 52419e95 already settled the separate question of clean-success builds,
routing a clean-success envelope to the `Temp/` file instead of the handoff. That
split is settled and is not reopened here; what remains is the non-clean-success
case.

Line numbers in `.agents/skills/compile/SKILL.md` may shift before this Plan
runs, because the prerequisite Plan named in this Plan's metadata edits the same
`## Handoff` section. Locate the text by the quoted wording, not by line number.

## Design

Recommended approach: delete the inline-verbatim requirement and let the fields
`SKILL.md` already mandates carry the whole inline report, then narrow the shared
exemption sentence to the artifacts and fields that still travel inline.

1. In `.agents/skills/compile/SKILL.md` `## Handoff`, remove the bullet requiring
   a build's envelope inline (currently `:69-72`). Replace it with nothing: the
   envelope-file bullet and the `Evidence` row already state where the verbatim
   bytes live, and the per-field bullets that follow (currently `:73-86`) already
   state everything the handoff reports inline. Keep every one of those field
   bullets exactly as they are — this Plan removes a duplicate copy of bytes, not
   any reported fact.
2. In `.agents/references/subagent-reporting.md`, narrow the typed-envelope
   exemption sentence (currently `:120-123`) so it no longer exempts a whole
   artifact main never decides on. The author's recommendation is to state the
   exemption over the verbatim bytes a consumer other than main requires when
   they must travel in a handoff, and to note that both named artifacts are
   normally file-routed at a cited path plus selector and so do not reach the
   caps. Do not delete the sentence: the typed receipts `/finalize-changes`
   consumes still need it for the fields
   `.agents/skills/finalize-changes/SKILL.md:82-96` and
   `.agents/skills/finalize-changes/references/scripts.md:10-16` route to main.
3. Check, before finishing, that the landing acceptance table's build row still
   resolves: `.agents/skills/finalize-changes/references/worker.md:76-78` reads
   the envelope from the cited `Temp/` path plus selector, so it must remain true
   that `/compile` writes that file and cites it. If the prerequisite Plan has
   already moved those mechanics into `.agents/skills/compile/references/worker.md`,
   verify the citing contract there instead.

If root-causing shows the correct fix reaches beyond the two files named in
`## In scope` — for example that the acceptance table needs a wording change to
keep resolving — surface it for re-planning instead of expanding scope.

## Critical files

- `.agents/skills/compile/SKILL.md` — `## Handoff`: the inline-verbatim bullet to
  remove and the field bullets to preserve.
- `.agents/references/subagent-reporting.md` — the typed-envelope exemption
  sentence in `## Handoffs`.
- `.agents/skills/finalize-changes/references/landing-acceptance-table.md` —
  read-only consumer whose build row must still resolve.
- `.agents/skills/finalize-changes/references/worker.md` — read-only: the
  verbatim-consumption requirement at `:76-78`.
- `.agents/skills/finalize-changes/references/scripts.md` — read-only: the
  receipt file-routing that keeps the receipt half of the exemption honest.

## In scope

- The `## Handoff` section of `.agents/skills/compile/SKILL.md`: remove the
  clause requiring a build's `broken-engine-build-result/v1` envelope verbatim
  inline, and change nothing else in that section.
- The typed-envelope exemption sentence in `.agents/references/subagent-reporting.md`
  `## Handoffs`: narrow it as `## Design` step 2 states.
- `/validate-skill` on the `compile` skill package and
  `/progressive-disclosure-review` on both changed files.

## Out of scope

- The clean-success versus failure split in `/compile`'s envelope reporting,
  settled by commit 52419e95.
- The `broken-engine-build-result/v1` schema, its fields, and its truncation
  behavior.
- Every `.agents/skills/compile/scripts/**` file and every other script.
- The field bullets at `.agents/skills/compile/SKILL.md:73-86`, the
  `Decisive checks`, `Evidence`, and `Residuals` narrowings that follow them, and
  `## Purpose`, `## When to use`, `## Inputs`, `### Structured build result`, and
  the frontmatter of that file.
- The shared handoff's field set, its row forms, and its
  10-row/40-line/20,000-character caps themselves — only the exemption sentence
  changes.
- Any other skill package's `## Handoff`, and any C++, GLSL, or
  project-membership change.

## Risk tier and invariants

Expected Tier 2 under the root `AGENTS.md` risk tiers; this author's
classification, to be confirmed at the Approve and classify step. The trigger is
scoped tool behavior: what one skill's dispatched worker returns. Escalate to
Tier 3 if the reviewer judges that editing the shared
`.agents/references/subagent-reporting.md` exemption sentence spans
independently owned surfaces, because every session reads that reference.

Invariants to preserve:

- Every fact `/compile` reports to main today is still reported: only the
  duplicate whole-envelope copy is removed.
- The verbatim envelope stays reachable verbatim by the `/finalize-changes`
  worker at a cited path plus selector.
- The landing acceptance table's build row still resolves.
- The receipt half of the exemption sentence keeps covering what
  `/finalize-changes` routes to main.
- Changed files keep their existing encoding and line endings.

## Acceptance criteria

- `.agents/skills/compile/SKILL.md` `## Handoff` no longer requires any build's
  envelope verbatim inline, and still requires the envelope file and the
  `Evidence` citation of it as path plus selector.
- Every field bullet the handoff mandated before the change is still mandated
  after it.
- The narrowed exemption sentence in `.agents/references/subagent-reporting.md`
  no longer exempts a whole artifact whose only verbatim consumer is not main,
  and still covers the receipt fields `/finalize-changes` routes to main.
- `/validate-skill` reports the `compile` package valid, and
  `/progressive-disclosure-review` raises no finding of this class against the
  changed files.
- `pwsh -NoProfile -File .agents/scripts/Test-PlanSchedulerState.ps1` reports
  `status: valid` and `code: ok`.

## Coordination

`Documents/Plans/ChangeWorkflow/CompileEnvelopeFileMechanicsToWorker.md` edits
the same `## Handoff` section on a different axis, relocating the envelope-file
mechanics into `.agents/skills/compile/references/worker.md`. That Plan is a
directional prerequisite of this one and is named in this Plan's metadata, so
this change re-reads the section as that change leaves it rather than assuming
the wording quoted here.

## Notes

The audit that proved this violation could not fix it: `/compile`'s envelope
reporting sat outside its approved boundary, and the coupled exemption narrowing
could not land while the inline clause still made the inline envelope legal.
