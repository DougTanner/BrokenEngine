<!-- broken-engine-plan/v1 {"createdUtc":"2026-09-05T22:29:56.860Z","dependsOn":[]} -->
# Fix: Find-SkillInboundReferences.ps1 — its header comment points at a validate-skill section and step number that do not exist

## Context
`.agents/skills/validate-skill/scripts/Find-SkillInboundReferences.ps1:1-2`
opens with "Raw inbound-reference sweep for one skill name over the fixed root
set in validate-skill's Workflow step 4."

Both halves of that pointer are false in the current tree. The
`validate-skill` package has no `## Workflow` section anywhere: `SKILL.md`
carries `## Purpose`, `## When to use`, `## Inputs`, `## Handoff`, and
`## References`, and the step list lives in
`.agents/skills/validate-skill/references/worker.md` under `## Steps`. The step
that actually runs this script is step 6 of that list
(`.agents/skills/validate-skill/references/worker.md:30`), not step 4; step 4
classifies the mechanical validator result and never mentions this script.

A reader following the comment therefore looks for a section that is not there,
and — if they map "Workflow" onto `## Steps` — lands on the wrong step. That is
a false navigation pointer of the kind `Documents/C++StyleGuide.txt` rule 64
excludes from comments, not a behavior defect: the script's own root set,
caps, and output are unaffected, and nothing reads the comment at runtime.

The condition is pre-existing at baseline commit
a53e71f7de18bcfa631aa8bfcce51842bbaf71b8. It was observed twice independently
in one session — by that session's `/validate-skill` reviewer (as a Recommended
finding) and by its `/progressive-disclosure-review` reviewer — while that
session was changing `validate-skill`'s `SKILL.md` `## Handoff` and its
`references/worker.md` step 7. The user approved that change with
`.agents/skills/validate-skill/scripts/` out of scope, so the comment was left
in place and recorded here instead.

## Design
Make the header comment state what the script does without depending on a step
number that can move.

The author's recommendation: keep the first sentence's description of the
script's job — a raw inbound-reference sweep for one skill name over a fixed
root set — and drop the "validate-skill's Workflow step 4" locator entirely,
since the script is only ever invoked from that one step and the caller already
knows where it stands. Keep the existing second sentence unchanged: the script
classifies nothing, and the validating agent decides which hits are invocation
requirements and which are user-typed examples.

An acceptable alternative, if a locator is judged worth keeping, is to name the
one that is true today — `references/worker.md` step 6 — accepting that it must
be renumbered whenever that step list changes. The implementing session picks
one; both are comment-text-only.

Nothing outside the comment block changes: no parameter, root set, cap, output
record, or exit path is touched.

## Critical files
- `.agents/skills/validate-skill/scripts/Find-SkillInboundReferences.ps1` —
  the header comment at `:1-4`
- `.agents/skills/validate-skill/references/worker.md` — `## Steps` step 6
  (`:30-37`), read-only; the true caller of this script
- `Documents/C++StyleGuide.txt` — rule 64, read-only; the comment-content rule
  the current text fails

## In scope
- The header comment lines at the top of
  `.agents/skills/validate-skill/scripts/Find-SkillInboundReferences.ps1`

## Out of scope
- Every executable line of that script: its parameters, root set, caps,
  returned record shape, per-root counts, `truncated` handling, and exit codes
- `.agents/skills/validate-skill/scripts/Validate-Skill.ps1` and any other
  bundled script
- `.agents/skills/validate-skill/SKILL.md` and
  `.agents/skills/validate-skill/references/worker.md`, including any
  renumbering of the `## Steps` list
- The other bundled `references/` files of that skill
- Unrelated skills and scripts; any transcript path or home path in the repo

## Risk tier and invariants
Expected Tier 1 (mechanical: a comment-only change with no public signature or
invariant exposure). Invariants to preserve: the script's observable behavior
is byte-identical before and after — same accepted parameter, same root set,
same records, same exit codes — and the file keeps its current encoding, line
endings, and trailing newline. Never embed a transcript path or a home path.

## Acceptance criteria
- The header comment, read on its own, describes the sweep without naming a
  section or step number that the `validate-skill` package does not contain
- `git diff` for this change touches only comment lines in that one file
- `/validate-skill` passes on `.agents/skills/validate-skill`; plan validate
  exits 0
