<!-- broken-engine-plan/v1 {"createdUtc":"2026-09-12T13:14:23.925Z","dependsOn":[]} -->
# Fix: four skills still require handoff enumerations the shared 10-row cap forbids inline

## Context
The shared form in `.agents/references/subagent-handoff.md` `## Handoffs` caps
any handoff field at 10 rows and requires the overflow to move to an existing
file or a `Temp/` file cited under `Evidence` as path plus selector, with a `##`
heading as the selector into a Markdown file. Four skills still declare handoff
rules that require per-file enumeration inline, so on a wide change the emitting
worker cannot satisfy both rules and floods main's context with per-file rows
main does not read there:

- `.agents/skills/resolve-findings/SKILL.md` lines 78-80: "Name each changed
  file once. Each shared `Build required` row names its target,
  configuration/platform, and the selected project-member `.cpp`; a changed
  header gets one row per consuming target and configuration/platform."
- `.agents/skills/add-collection/references/worker.md` step 18, lines 137-139:
  "Return an exact `Build required` request for every affected client/server
  target, naming configuration/platform and each selected project-member
  `.cpp`; for changed headers, name every consuming target."
- `.agents/skills/reduce-file/references/worker.md` step 10, lines 69-71:
  "Return exact `Build required` rows for every affected target, naming
  configuration/platform and each selected project-member `.cpp`; for changed
  headers, name every consuming target."
- `.agents/skills/update-affected-code/SKILL.md` lines 63-64 and 76: "Each
  shared `Build required` row names the exact target, configuration/platform,
  and project-member path, using `none` when absent." and "Name each changed
  file once."

The same conflict was proven and fixed in `.agents/skills/implement-plan/SKILL.md`
`## Handoff`; that change's `## Out of scope` excluded "any other skill", so
these four sites are a proven out-of-scope leftover rather than an unmet
acceptance criterion of that change. They were found by
`rg "selected project-member|Name each changed file once" .agents`.

## Design
The author's recommendation is to mirror the shape already applied to
`/implement-plan`'s `## Handoff`, whose current repository text is the
reference implementation to read before editing:

- `Build required` keeps each distinct target and that target's
  configuration/platform inline in every case.
- Only the per-`.cpp` selected project-member path, and for a changed header
  its list of consuming targets, move with the overflow.
- When the changed set would push a field past the shared row cap, `Changed
  files` carries the count and the per-file rows move to the file cited under
  `Evidence` as path plus `##` selector.
- Build requests stay executable without rediscovery wherever the per-`.cpp`
  detail sits, inline or in that cited file.
- The overflow file's own shape is not restated; each skill points at the
  shared form for it.

`/implement-plan` also adjusted the one step in its
`references/worker.md` whose done-condition repeated the superseded rule; the
fix session should make the same single-reference repair in each of the four
packages only where a changed rule leaves an existing reference incorrect.

Make the smallest fix inside the `## In scope` boundary below. If the work
would reach the shared form itself, surface it for re-planning instead of
expanding scope.

## Critical files
- `.agents/skills/resolve-findings/SKILL.md` (`## Handoff`)
- `.agents/skills/add-collection/references/worker.md` (step 18)
- `.agents/skills/reduce-file/references/worker.md` (step 10)
- `.agents/skills/update-affected-code/SKILL.md` (`## Handoff`)
- `.agents/skills/implement-plan/SKILL.md` (`## Handoff`) — read as the
  reference implementation; changing it is out of scope
- `.agents/references/subagent-handoff.md` (`## Handoffs`) — read as the
  authoritative shared form; changing it is out of scope

## In scope
- `.agents/skills/resolve-findings/SKILL.md`: the sentences "Name each changed
  file once." and "Each shared `Build required` row names its target,
  configuration/platform, and the selected project-member `.cpp`; a changed
  header gets one row per consuming target and configuration/platform."
- `.agents/skills/add-collection/references/worker.md`: the step 18 sentence
  "Return an exact `Build required` request for every affected client/server
  target, naming configuration/platform and each selected project-member
  `.cpp`; for changed headers, name every consuming target." and its
  done-condition sentence if the rewrite makes it incorrect.
- `.agents/skills/reduce-file/references/worker.md`: the step 10 sentence
  "Return exact `Build required` rows for every affected target, naming
  configuration/platform and each selected project-member `.cpp`; for changed
  headers, name every consuming target." and its done-condition sentence if the
  rewrite makes it incorrect.
- `.agents/skills/update-affected-code/SKILL.md`: the sentences "Each shared
  `Build required` row names the exact target, configuration/platform, and
  project-member path, using `none` when absent." and "Name each changed file
  once."

## Out of scope
- `.agents/references/subagent-handoff.md` and the shared form's caps
- `.agents/skills/implement-plan/SKILL.md` and its `references/worker.md`,
  already corrected
- `.agents/skills/compile/SKILL.md`, recorded separately
- Any other rule in the four touched files, and any other skill
- Restating the overflow file's shape in any skill

## Risk tier and invariants
Tier 1 (mechanical): skill documentation shaping a handoff contract, with no
public signature or invariant exposure. Escalate only if the fix reaches the
shared handoff form. No unit tests.

## Acceptance criteria
- None of the four files requires an enumeration that would exceed the shared
  10-row cap inline; each states the overflow route — count plus the file cited
  under `Evidence` as path plus `##` selector — and keeps distinct targets with
  configuration/platform inline.
- `/external-skill-creator` validation of each changed skill package reports no
  new finding.
- The static-checks runner, invoked as `.agents/references/static-checks.md`
  documents it, reports every row the change triggers passing.

## Notes
Recorded as an ordinary debt follow-up for an out-of-scope leftover of the
`/implement-plan` handoff fix; the cost is main-session context efficiency on
wide changes.
