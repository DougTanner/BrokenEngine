<!-- broken-engine-plan/v1 {"createdUtc":"2026-09-07T18:15:08.879Z","dependsOn":[]} -->
# Switch four per-item verdict handoff fields to the count-line-plus-exceptions shape

## Context

`.agents/references/skill-skeleton.md` `## Section order` item 5 now states the
required shape for a handoff field that judges each item of a supplied list: the
field carries a `<n>/<total>` count line closed by the field's own passing word,
followed only by the items that did not pass, each citing where its settling
evidence lives as a path plus selector or as the handoff row that holds it, and
never restating that evidence.

`.agents/skills/verify-acceptance/SKILL.md` `## Handoff` already applies that
shape to its `Criteria` field, and is the worked example this Plan follows.

Four other skill handoff fields still require one row per supplied item with a
per-item verdict token, so a run over a large list returns a field that both
floods the manager with rows it takes no action on and can exceed the 10-row
per-field cap in `.agents/references/subagent-reporting.md` `## Handoffs`:

- `.agents/skills/agent-harness/SKILL.md` `Criterion results` — "one row per
  acceptance criterion", row form `<criterion ID> — PASS | FAIL | BLOCKED —
  command/query/scene/UI and evidence selector, or HARNESS-F-### when that
  finding holds the evidence`.
- `.agents/skills/plan-audit/SKILL.md` `Traceability checked` — "one
  single-line row per mapping" of requirement or invariant to implementation
  site or check.
- `.agents/skills/update-affected-code/SKILL.md` `Trigger outcomes` — "one
  outcome per trigger: `RESOLVED` ... `REFUTED` ... or `UNRESOLVED`".
- `.agents/skills/update-vcxproj/SKILL.md` — the unnamed per-path membership
  outcome extension, "one row per path", row form ending
  `verified|fixed|NOTE <detail>|FAIL <detail>`.

Evidence for all four, with the exact quoted text and line numbers at baseline
`fd6c7dc842aa888b5555c6eab773306c12a11039`, is the survey in
`Temp/PerItemVerdictFields.md` under the matching `## <skill>` headings. That
file is gitignored scratch and may not survive; the four `## Handoff` sections
named above are the durable evidence and are what this Plan changes.

This follow-up was directed by the user in the session that made the
skill-skeleton and `/verify-acceptance` edits, so those two files are already
correct and are out of scope here.

## Design

Recommended approach: give each of the four fields a count line plus only the
non-passing rows, exactly as `/verify-acceptance` `Criteria` does, using each
field's own vocabulary rather than forcing the word "passed" onto a field that
does not use it. Rationale: the manager's action on each of these fields is to
route the items that still need a decision; a settled item's row costs context
without changing any routing, and the count line preserves the completeness
proof the enumeration was there to give.

Per field, the author's recommendation is:

- `Criterion results` (`/agent-harness`) — count line `<passed>/<total>
  passed`, then only the `FAIL` and `BLOCKED` criteria, each keeping its
  verdict token and its evidence citation (an evidence selector or a
  `HARNESS-F-###` ID). The existing `Status` derivation sentence stays true
  because it already keys on the presence of `FAIL` and `BLOCKED` results.
- `Traceability checked` (`/plan-audit`) — count line `<traced>/<total>
  traced`, then only the requirements or invariants the audit could not trace
  to an implementation site or check, each citing where its settling evidence
  lives. Rationale for the vocabulary: this field has no verdict token today,
  and "traced" is the property the rows actually assert.
- `Trigger outcomes` (`/update-affected-code`) — count line `<settled>/<total>
  settled`, where a settled trigger is `RESOLVED` or `REFUTED`, then only the
  `UNRESOLVED` triggers with their owner and action. Rationale: both `RESOLVED`
  and `REFUTED` are outcomes the manager takes no further action on, so listing
  them is the cost item 5 removes; the alternative of counting only `RESOLVED`
  and listing `REFUTED` rows is recorded in `## Notes`.
- The per-path membership outcome (`/update-vcxproj`) — count line
  `<settled>/<total> settled`, where a settled path is `verified` or `fixed`,
  then only the `NOTE` and `FAIL` paths on the existing row form. The existing
  `Residuals` sentence ("a FAIL, conflict, or NOTE requiring action") stays
  true.

Each of the four `SKILL.md` `## Handoff` sections also applies item 5's evidence
rule to its listed rows in that field's own terms: the row cites where its
settling evidence lives as a path plus selector or as the handoff row that holds
it, and never restates it.

Each of the four skills' `references/worker.md` gets only the wording needed to
stay true to the new field shape — nothing more. At baseline, a search of the
four `references/worker.md` files for the field names and their verdict tokens
found matches only in `.agents/skills/update-vcxproj/references/worker.md`
(`report NOTE`, `leave any unresolved invariant as FAIL`) and in
`.agents/skills/agent-harness/references/worker.md` (`BLOCKED` reporting
instructions); those keep their meaning under the new shape, so the expected
worker-file delta is zero or near zero. The implementer confirms this against
the tree rather than assuming it.

Risk tier: Tier 2. Trigger: these edits change an output/handoff contract, which
`.agents/skills/plan-simplicity-review/SKILL.md` `### Trigger: when a skill edit
is behavior` classifies as behavior rather than documentation. No
determinism/CRC, wire, serialization, replay, threading, or trust-boundary
surface is touched, and no C++, GLSL, or project membership changes, so this
stays inside Tier 2.

Invariants to preserve:

- Every changed `## Handoff` still conforms to
  `.agents/references/skill-skeleton.md` `## Section order` item 5 and to the
  size caps and extension rules in `.agents/references/subagent-reporting.md`
  `## Handoffs`; in particular no changed section re-renders the shared form,
  `Build required` stays present, and `Residuals` stays last where the skill
  declares them.
- Each skill's existing `Status` derivation and `Residuals` wording remains true
  after the field shape changes; where it would not be, it is corrected in the
  same edit.
- Progressive disclosure: the count-line-plus-exceptions rule itself is stated
  once in `.agents/references/skill-skeleton.md` and is applied, not restated as
  a general rule, in each `SKILL.md`.

## Critical files

- `.agents/skills/agent-harness/SKILL.md` — `## Handoff`, the `Criterion
  results` bullet and its row-form fence.
- `.agents/skills/plan-audit/SKILL.md` — `## Handoff`, the `Traceability
  checked:` line of the returned extension-field fence.
- `.agents/skills/update-affected-code/SKILL.md` — `## Handoff`, the `Trigger
  outcomes` bullet.
- `.agents/skills/update-vcxproj/SKILL.md` — `## Handoff`, the per-path
  membership outcome sentence and its row-form fence.
- `.agents/skills/agent-harness/references/worker.md`,
  `.agents/skills/plan-audit/references/worker.md`,
  `.agents/skills/update-affected-code/references/worker.md`,
  `.agents/skills/update-vcxproj/references/worker.md` — only wording that
  becomes false under the new field shape.
- `.agents/references/skill-skeleton.md` `## Section order` item 5 — the
  governing rule; read, not changed.
- `.agents/skills/verify-acceptance/SKILL.md` `## Handoff` — the worked
  example; read, not changed.

## In scope

- `.agents/skills/agent-harness/SKILL.md` `## Handoff`: replace the `Criterion
  results` field's "one row per acceptance criterion, on the row form below"
  clause and its row-form fence with a `<passed>/<total> passed` count line
  followed only by the `FAIL` and `BLOCKED` criterion rows, each citing its
  settling evidence as a path plus selector, a `Decisive checks` row, or a
  `HARNESS-F-###` ID.
- `.agents/skills/plan-audit/SKILL.md` `## Handoff`: replace the `Traceability
  checked:` line's "return one single-line row per mapping" clause with a
  `<traced>/<total> traced` count line followed only by the untraced
  requirements or invariants, each citing its settling evidence.
- `.agents/skills/update-affected-code/SKILL.md` `## Handoff`: replace the
  `Trigger outcomes` field's "one outcome per trigger" clause with a
  `<settled>/<total> settled` count line, settled meaning `RESOLVED` or
  `REFUTED`, followed only by the `UNRESOLVED` triggers with owner, action, and
  evidence citation.
- `.agents/skills/update-vcxproj/SKILL.md` `## Handoff`: replace the per-path
  membership outcome's "one row per path on this form" clause with a
  `<settled>/<total> settled` count line, settled meaning `verified` or
  `fixed`, followed only by the `NOTE` and `FAIL` paths on the existing row
  form, each citing its settling evidence.
- In the same four `## Handoff` sections only: adjust any adjacent sentence —
  `Status` derivation, `Residuals` content, or a "return the complete report
  inline" instruction — that the new field shape would otherwise make false.
- In the four skills' `references/worker.md` files only: the minimum wording
  needed to stay true to the new field shape, where such wording exists.

## Out of scope

- The per-claim verdict fields `Per-proposition verdicts` in
  `.agents/skills/verify-external-claims/SKILL.md` and `External claim
  verdicts` in `.agents/skills/external-grill-plan/SKILL.md`, and the item table
  in `.agents/skills/resolve-findings/SKILL.md` `## Handoff`. The manager acts
  on every row of those three — each verified claim is an input to a dependent
  decision, and each resolved item is a region to re-review — so suppressing
  the passing rows would remove material the manager uses.
- `.agents/skills/compile/SKILL.md`, `.agents/skills/implement-plan/SKILL.md`,
  `.agents/skills/next-plan-review/SKILL.md`, and every other skill named in
  the `Temp/PerItemVerdictFields.md` survey.
- `.agents/references/skill-skeleton.md` and
  `.agents/skills/verify-acceptance/SKILL.md`, which already carry the target
  shape.
- `.agents/references/subagent-reporting.md`, including its size caps and its
  `What main does with each field` table.
- Any change to what the four skills do, verify, or check; only how they report
  per-item results changes.
- Any C++, GLSL, shader, project-membership, or script change.

## Acceptance criteria

- Each of the four `## Handoff` sections declares its field as a count line plus
  only the non-passing rows, with the count-line vocabulary this Plan's
  `## Design` recommends for that field, and states the evidence-citation rule
  for the listed rows.
- No changed `## Handoff` section still instructs a worker to return one row per
  supplied item for these four fields.
- `Status` derivation and `Residuals` wording in each changed `SKILL.md` remain
  consistent with the new field shape.
- `/validate-skill` passes for each of the four changed skill packages.
- `/progressive-disclosure-review` reports no finding that a changed section
  restates `.agents/references/skill-skeleton.md` item 5 or
  `.agents/references/subagent-reporting.md`.
- No file outside `## In scope` is changed.

## Notes

- Alternative considered for `Trigger outcomes`: count only `RESOLVED` and list
  both `REFUTED` and `UNRESOLVED` rows. The author recommends against it
  because a refuted trigger is a settled outcome the manager takes no action
  on, so listing it reintroduces exactly the rows item 5 removes; the
  implementer may raise this with the user if evidence shows managers do act on
  refutations.
- The four fields differ in vocabulary, so the implementer should resist
  normalizing them to a single word; item 5's shape is the requirement, not a
  shared token set.
