<!-- broken-engine-plan/v1 {"createdUtc":"2026-09-06T16:05:46.320Z","dependsOn":[]} -->
# Fix: `/validate-skill` — `## Inputs` never names the required `-Path` parameter

## Context

`.agents/skills/validate-skill/SKILL.md:28` states the skill's input as "Accept
one repository skill directory or its `SKILL.md`. The optional Codex
`agents/openai.yaml` is validated with the package. Disposable fixtures may be
outside `.agents/skills/` only with `-Fixture`." It names `-Fixture` but never
names `-Path`, so the public file reads as if the target travels bare.

The validator requires the named parameter. `Validate-Skill.ps1` binds
`[CmdletBinding(PositionalBinding = $false)]` with a single
`ValueFromRemainingArguments` collector
(`.agents/skills/validate-skill/scripts/Validate-Skill.ps1:13-17`) and then parses
the collected arguments itself: only a literal `-Path` or `-Fixture` token is
accepted, and any other token falls through to
`Write-SetupError 'INVOCATION' 'provide exactly one -Path target'`
(`.agents/skills/validate-skill/scripts/Validate-Skill.ps1:42-57`), which exits 2
(`:22-28`).

Observed this session: running the bare positional form

`pwsh -NoProfile -File .agents/skills/validate-skill/scripts/Validate-Skill.ps1 .agents/skills/<package>`

exits 2 with `SETUP_ERROR INVOCATION: provide exactly one -Path target`, while
the same command with `-Path .agents/skills/<package>` exits 0. The failure cost
one wasted run and a re-read before the correct form was found.

The full invocation is already documented at its owning layer —
`.agents/skills/validate-skill/references/worker.md:16` and
`.agents/skills/validate-skill/references/frontmatter-schema.md:87` both spell it
with `-Path` — so this is a naming gap in the public file, not a missing
mechanic. Nothing is broken for a worker that reads the reference; the cost falls
on a reader who invokes from `## Inputs` alone.

## Design

Recommended approach: name the parameter in `## Inputs` and change nothing else.

Reword `.agents/skills/validate-skill/SKILL.md:28` so the accepted target is
stated as the value of a required `-Path` parameter — for example, "Accept one
repository skill directory or its `SKILL.md`, passed as `-Path`." — keeping the
existing `agents/openai.yaml` and `-Fixture` sentences as they stand.

Do not copy the full command line into `SKILL.md`: the root `AGENTS.md`
progressive-disclosure directive puts the invocation in the references that
already carry it, and duplicating it would create a second site to maintain. The
`## Inputs` section only has to stop implying a bare positional target.

Do not change the script. Its rejection of an unnamed target is deliberate — the
parser treats any unrecognized token as a setup error so a mistyped option can
never be read as a path — and no evidence in this Plan argues against it. If the
implementing session concludes the script should accept a positional target
instead, that is a behavior change outside this boundary: surface it for
re-planning rather than making it here.

## Critical files

- `.agents/skills/validate-skill/SKILL.md` — `## Inputs`, the file to change.
- `.agents/skills/validate-skill/scripts/Validate-Skill.ps1` — read-only: the
  parameter contract at `:13-17` and `:42-57` the wording must match.
- `.agents/skills/validate-skill/references/worker.md` — read-only: the
  documented invocation at `:16`.
- `.agents/skills/validate-skill/references/frontmatter-schema.md` — read-only:
  the documented invocation at `:87`.

## In scope

- The `## Inputs` section of `.agents/skills/validate-skill/SKILL.md`: name the
  required `-Path` parameter as the carrier of the accepted target.
- `/validate-skill` on its own package after the edit.

## Out of scope

- `.agents/skills/validate-skill/scripts/Validate-Skill.ps1` and every other
  script: no parameter, parsing, exit-code, or message change.
- `## Purpose`, `## When to use`, `## Handoff`, `## References`, and the
  frontmatter of `.agents/skills/validate-skill/SKILL.md`.
- `.agents/skills/validate-skill/references/**`, which already document the full
  invocation correctly.
- Copying the full command line into `SKILL.md`.
- Every other skill package, and any C++, GLSL, or project-membership change.

## Risk tier and invariants

Expected Tier 1 under the root `AGENTS.md` risk tiers; this author's
classification, to be confirmed at the Approve and classify step. The trigger is
mechanical documentation work: one sentence in one public skill file, with no
public signature, script behavior, or invariant exposure.

Invariants to preserve:

- The validator's behavior is unchanged; only the documentation matches it.
- Progressive disclosure: the full invocation still lives once, in the
  references that already carry it.
- The `SKILL.md` keeps its existing encoding and line endings, and its
  frontmatter is untouched.

## Acceptance criteria

- `.agents/skills/validate-skill/SKILL.md` `## Inputs` names `-Path` as the
  parameter carrying the skill directory or `SKILL.md` target.
- `## Inputs` still states the `agents/openai.yaml` and `-Fixture` facts it
  states today, and no full command line was added to `SKILL.md`.
- The diff touches only `.agents/skills/validate-skill/SKILL.md`.
- `pwsh -NoProfile -File .agents/skills/validate-skill/scripts/Validate-Skill.ps1 -Path .agents/skills/validate-skill`
  exits 0.
- `pwsh -NoProfile -File .agents/scripts/Test-PlanSchedulerState.ps1` reports
  `status: valid` and `code: ok`.

## Notes

Found while auditing skill handoff sections; the wording gap is unrelated to that
audit's subject, so it is recorded here instead of fixed there.
