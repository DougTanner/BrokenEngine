<!-- broken-engine-plan/v1 {"createdUtc":"2026-09-07T17:59:22.593Z","dependsOn":[]} -->
# Fix: skill `## Handoff` sections re-declare shared handoff fields

## Context

`.agents/references/subagent-reporting.md` `## Handoffs` owns the eight shared
handoff fields (`Status`, `Findings`, `Changed files`, `Decisive checks`,
`Build required`, `Evidence`, `Executor`, `Residuals`) and states that a skill
extends the form "only by adding rows inside an existing field or by declaring
extra fields in its own `## Handoff` section", "never by re-rendering the form
itself". `.agents/references/skill-skeleton.md` `## Section order` item 5
repeats that boundary for the `## Handoff` section.

Many `## Handoff` sections nonetheless present shared fields as bullets in the
same list, and in the same backticked-name-then-dash shape, as the skill's own
extension fields. A worker reading such a list has no way to tell an extension
field it must add from a shared field already present, so it renders the shared
field a second time below the form.

Observed symptom (the session that produced this Plan): the `/resolve-findings`
worker returned `Build required: none` and `Residuals: none` inside the shared
form and again as `- Build required — none.` and `- Residuals — none.` in the
bullet list after the skill's item table. Two fields appeared twice and the
shared `Residuals` was no longer last, which the manager had to reconcile before
routing the handoff. That session fixed `/resolve-findings` only; the same
bullet shape remains in the other skills, and
`.agents/skills/implement-plan/SKILL.md:59-61` still carries the byte-identical
`Build required` bullet that the `/resolve-findings` text was derived from.

Enumeration of the current tree (run from the worktree root):

```sh
grep -rn '^- `\(Status\|Findings\|Changed files\|Decisive checks\|Build required\|Evidence\|Executor\|Residuals\)`' .agents/skills --include=SKILL.md
```

At the time of writing it returns 33 bullets across the 18 `SKILL.md` files
listed under `## In scope`. One of the 33,
`.agents/skills/create-follow-up-plans/SKILL.md:27`, sits in that file's
`## Inputs` section rather than its `## Handoff` section, so it is outside this
Plan's boundary and is not to be changed; the other 32 are inside `## Handoff`
sections and are the bullets in scope. Re-run the command during
implementation, discarding any match outside a `## Handoff` section the same
way; the `## In scope` list is the authority for which files are in the
boundary, and a bullet that has since been removed is simply nothing to do.

Two further defects sit in the same `## Handoff` section of
`.agents/skills/resolve-findings/SKILL.md`, both pre-existing and explicitly
excluded by that session's Plan boundary:

- Its item table's `Focused check` column carries a check and its result, which
  is what the shared `Decisive checks` field carries;
  `.agents/references/subagent-reporting.md` `## Handoffs` says "do not repeat a
  row from another field".
- That table's `FIXED` and `UNRESOLVED` cell values are status words outside the
  shared vocabulary; `.agents/references/skill-skeleton.md` `## Shared
  vocabulary` allows only the status and severity words the shared form lists
  and says "Introduce no other status or severity term".

## Design

Apply one convention across the `## Handoff` sections named below, the form
`/resolve-findings` adopted in the originating session and that
`add-collection`, `add-collection-member`, `next-plan-checkpoint-review`, and
`finalize-changes` already use: a skill declares as a bullet only a field the
shared form does not own, and keeps any non-shared detail about a shared field
as row-form guidance in prose that names the field as shared — for example
`/resolve-findings`'s "Each shared `Build required` row names its target,
configuration/platform, and the selected project-member `.cpp`".

The author's recommendation is to classify each enumerated bullet into exactly
one of three outcomes and apply it:

1. It restates only what the shared form already says (for example "`Residuals`
   — unresolved item, or none; last."). Delete the bullet; the shared form
   already carries that text.
2. It narrows the field with skill-specific detail (for example `/compile`'s
   per-build `Decisive checks` row form). Move that detail into prose in the
   same section, phrased as guidance for the shared field, and delete the
   bullet.
3. It fixes the field's value for this skill, which
   `.agents/references/skill-skeleton.md` `## Section order` item 5 permits as a
   "fixed shared value" (for example `/create-follow-up-plans`'s "`Findings`:
   none."). Keep it, but make it unambiguous that it states a value rather than
   declares a field.

Where a section mixes extension-field bullets with shared-field bullets, the
surviving extension bullets stay a bullet list and the shared-field guidance
becomes prose beneath it, so the two are never the same shape.

For the two `/resolve-findings` item-table defects, the author's
recommendations are: drop the `Focused check` column and let the shared
`Decisive checks` field carry the check and its result, since the table's
per-item mapping is already available through the item name; and replace the
`FIXED`/`UNRESOLVED` cell values with wording that carries the same meaning
without introducing status words — stating the fixed region, or `none` with the
reason, is enough to distinguish the two cases. Confirm both against the two
governing references before editing, and if either fix would require changing a
governing reference, surface it for re-planning rather than expanding scope.

## Critical files

- `.agents/references/subagent-reporting.md` — `## Handoffs`, read-only
  authority for the shared form and its extension rule
- `.agents/references/skill-skeleton.md` — `## Section order` item 5 and
  `## Shared vocabulary`, read-only authority for the `## Handoff` shape
- `.agents/skills/resolve-findings/SKILL.md` — `## Handoff`, both the exemplar
  prose form and the two remaining item-table defects

## In scope

- The `## Handoff` section of each of these `SKILL.md` files under
  `.agents/skills/`, limited to bullets naming a shared handoff field and the
  prose that replaces them: `adversarial-review`, `agent-harness`,
  `analyze-diagsession`, `code-style-review`, `compile`,
  `create-follow-up-plans`, `external-diagnose-bug`, `external-grill-plan`,
  `glsl-review`, `implement-plan`, `next-plan-review`, `reduce-file`,
  `repo-code-review`, `session-audit`, `update-affected-code`,
  `update-claude-docs`, `update-vcxproj`, `validate-skill`
- `.agents/skills/resolve-findings/SKILL.md` `## Handoff`: the item table's
  `Focused check` column and its `FIXED`/`UNRESOLVED` cell values only

## Out of scope

- `.agents/references/subagent-reporting.md`,
  `.agents/references/skill-skeleton.md`, and the shared handoff form itself
- Every section of the listed `SKILL.md` files other than `## Handoff`, and
  every `references/worker.md`
- Extension fields the shared form does not own, and the `## Handoff` sections
  of skills the enumeration does not return
- `/coherence-review`'s `## Handoff`, which
  `Documents/Plans/ChangeWorkflow/CoherenceReviewCriteriaFieldCap.md` owns
- Changing what any skill actually returns beyond removing the duplication —
  no field gains or loses required content

## Risk tier and invariants

Expected Tier 2: the edits change output/handoff contracts, which
`/plan-simplicity-review` `### Trigger: when a skill edit is behavior`
classifies as behavior rather than documentation, and the whole sweep stays
inside one artifact type with no determinism, wire, serialization, threading, or
build/bootstrap exposure. Escalate to Tier 3 if implementation finds the fix
requires editing a governing reference or changes what main routes on.

Invariants: the shared form in `.agents/references/subagent-reporting.md` stays
the single authority; every handoff still carries `Build required` and still
ends with `Residuals`; no field appears twice in a returned handoff; no status
or severity word outside the shared vocabulary is introduced; each skill's
required handoff content is preserved, only its location and shape change.

## Acceptance criteria

- The enumeration command in `## Context` returns no bullet in any `## Handoff`
  section of the listed files, except bullets that state a fixed shared value
  under outcome 3 of `## Design`.
- Every piece of skill-specific detail a removed bullet carried is present in
  the same `## Handoff` section as prose naming the shared field, or is recorded
  as a deliberate drop in the change report.
- `.agents/skills/resolve-findings/SKILL.md` `## Handoff` no longer carries a
  column duplicating the shared `Decisive checks` field, and introduces no
  status word outside the shared vocabulary.
- The static pass `.agents/references/static-checks.md` triggers for a change
  under `.agents/skills/` returns a `broken-engine-static-checks/v1` envelope
  whose `validate-skill` row passes for every changed skill package.

## Notes

The originating session fixed `/resolve-findings`'s `Build required` and
`Residuals` bullets only; its Plan is completed and deleted, so there is no
dependency edge to it. Its landed diff is the worked example for the prose form
this Plan generalizes.
