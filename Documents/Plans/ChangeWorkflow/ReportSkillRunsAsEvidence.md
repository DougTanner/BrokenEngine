<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-24T16:07:35.644Z","dependsOn":[]} -->
# Move the skill-run-as-evidence reporting rules into the run skills' own handoffs

## Context

Until the change that created this Plan, `.agents/references/subagent-reporting.md`
`## Task brief` carried one paragraph (lines 44-51 at commit `0842b7633`, readable
with `git show 0842b7633:.agents/references/subagent-reporting.md`) telling the
main session how to write a brief that has a worker run another skill one or more
times as acceptance evidence. That change removes the paragraph. The authoring
agent's verbatim record of the user's words in that session: "I think it should be removed, but these
'which result to report, give one summary row per run, and save the full results
to a scratch file' should be part of the skills instructions themselves (and
enforced by the skill-creator skill)". This Plan does that second part, which
was deferred because it touches several skill packages.

The removed paragraph's requirements, restated here because the paragraph no
longer exists:

1. Decisive field — the report names the one field of the run skill's handoff
   that the acceptance criterion turns on.
2. One summary row per run — under the worker's shared `Decisive checks` field,
   one row per run carrying whether the expected observation was met, that
   run's `Status`, and the row of the decisive field; plus one row describing the
   resulting working-tree state, summarized, never pasting `git status --porcelain`
   output inline.
3. Full results in a scratch file — each run's complete handoff is written to
   one gitignored `Temp/` file, one `##` heading per run (the paragraph used
   `## Run <n>`), cited under the worker's `Evidence` as path plus heading.

Skills whose handoff another skill's instructions already have a worker run
inside that worker's own task (rather than as a separate dispatch):

- `/external-skill-creator` validate mode and `/progressive-disclosure-review` —
  run by the `/coherence-review` Tier-1 combined pass
  (`.agents/skills/coherence-review/references/worker.md` step 5).
- `/agent-harness` and `/compile` — run by the `/external-diagnose-bug` worker as
  reproduction evidence (`.agents/skills/external-diagnose-bug/references/worker.md`
  step 2, items 1 and 3); `/agent-harness` is also the usual skill a brief has a
  worker run repeatedly, as in a replay determinism check.

`/coherence-review` `## Handoff` already states its own equivalent for the
skills it runs: each triggered skill's complete handoff goes verbatim into one
`Temp/` file with one selector per skill, and its `Apply the triggered cleanup`
field carries one inline row per skill.

## Design

The author recommends placing the rule on the run skill's side: each skill
above gains, in its `## Handoff`, one short paragraph for when a worker runs it
inside another task as evidence. That paragraph names the skill's decisive
field and states requirements 2 and 3. Rationale: a worker that runs a skill
reads that skill's `SKILL.md`, so the rule reaches both skill-encoded runs and
runs a brief assigns, without restoring a brief-level rule; and only the run
skill knows which of its fields decides a result.

Recommended decisive fields, taken from each skill's current `## Handoff`:

- `/agent-harness` — the `Criterion results` field.
- `/compile` — the per-build `Decisive checks` row (`status`, `exitCode`).
- `/external-skill-creator` validate mode — `Status`.
- `/progressive-disclosure-review` — `Status`.

The paragraph writes requirement 3 as "one `##` heading per run" rather than the
literal `## Run <n>`, so it stays consistent with the one-selector-per-skill
rule `/coherence-review` already states; `/coherence-review` itself is left
unchanged.

Enforcement, recommended as a semantic check rather than a `Validate-Skill.ps1`
rule, because whether an instruction runs a skill inside another task is a
reading judgment, not a text pattern:

- `.agents/skills/external-skill-creator/references/semantic-review.md` step 6
  already collects and classifies inbound references. Extend its classification:
  when an inbound reference has a worker run the target skill inside that
  worker's own task, the target's `## Handoff` must carry the run-as-evidence
  paragraph with all three requirements. A missing or incomplete paragraph is a
  Critical finding, because the user asked for these requirements to be
  "enforced by the skill-creator skill" (quoted under `## Context`), and in
  `/external-skill-creator` only a Critical finding blocks `PASS`; a Recommended
  one is advisory (`.agents/skills/external-skill-creator/SKILL.md` `## Handoff`).
- That file's `## Rules` bullet "Do not promote ordinary quality advice to
  Critical unless discovery or invocation is concretely incorrect" would forbid
  that severity, so it gains one exception naming the step 6 run-as-evidence
  check; the rest of the rule is unchanged.
- `.agents/skills/external-skill-creator/references/authoring.md`
  `### Repository Conventions` gains one bullet so author mode writes the
  paragraph when a new or revised skill is run inside another task.

Risk tier: Tier 2. Trigger: scoped tool behavior — the handoff contracts of four
skills and the validate-mode behavior of `/external-skill-creator`, all within
the Change Workflow tooling. No determinism/CRC, serialization, wire, replay,
threading, trust, or build/bootstrap surface is reached; `Validate-Skill.ps1`
and every script stay unchanged.

## Critical files

- `.agents/skills/agent-harness/SKILL.md` — `## Handoff`
- `.agents/skills/compile/SKILL.md` — `## Handoff`
- `.agents/skills/external-skill-creator/SKILL.md` — `## Handoff`, validate mode
- `.agents/skills/progressive-disclosure-review/SKILL.md` — `## Handoff`
- `.agents/skills/external-skill-creator/references/semantic-review.md` — step 6,
  `## Rules`
- `.agents/skills/external-skill-creator/references/authoring.md` —
  `### Repository Conventions`

## In scope

- One run-as-evidence paragraph appended to the `## Handoff` section of each of
  the four `SKILL.md` files above, naming that skill's decisive field and stating
  requirements 2 and 3.
- The inbound-reference classification sentence of `semantic-review.md` step 6,
  extended with the check and its Critical severity.
- The `semantic-review.md` `## Rules` bullet "Do not promote ordinary quality
  advice to Critical unless discovery or invocation is concretely incorrect",
  amended only by appending the exception "or the finding is the step 6
  run-as-evidence check".
- One bullet in `authoring.md` `### Repository Conventions`.

## Out of scope

- `.agents/references/subagent-reporting.md` and
  `.agents/references/subagent-handoff.md` — no brief-level or shared-form rule
  is reintroduced.
- `.agents/skills/coherence-review/` and `.agents/skills/external-diagnose-bug/`
  — the running skills keep their current text.
- `.agents/skills/external-skill-creator/scripts/Validate-Skill.ps1`,
  `frontmatter-schema.md`, and every other script or mechanical rule.
- Any skill other than the four named. When the step 6 sweep run during this
  change's own validation reports another skill run inside a worker's task, that
  skill is reported as a residual, not edited.

## Acceptance criteria

- Each of the four `SKILL.md` `## Handoff` sections carries the paragraph, and it
  states all three restated requirements: its decisive field, one `Decisive checks`
  row per run plus one working-tree-state row with no inline
  `git status --porcelain` output, and the complete handoff in one `Temp/` file
  under one `##` heading per run cited under `Evidence`.
- `semantic-review.md` step 6 reports a missing or incomplete paragraph on a
  skill run inside another worker's task as a Critical finding, and the
  `## Rules` bullet on promoting advice to Critical permits that severity.
- `/external-skill-creator` validate mode returns `PASS` for each of the four
  changed packages and for `external-skill-creator` itself.

## Notes

- Verification route: the Change Workflow at Tier 2 — `/external-skill-creator`
  validate mode for every changed package and `/progressive-disclosure-review`
  for the changed instruction prose, then `/finalize-changes`. No build or
  `/agent-harness` run applies.
- The paragraph repeats in four skills by the user's words recorded verbatim
  under `## Context`; a `/progressive-disclosure-review` finding that the per-skill
  repetition is duplication conflicts with those words, and is surfaced as a
  residual naming both sides (root `AGENTS.md` `### Diagnosis Discipline`)
  rather than resolved by removing the repetition.
