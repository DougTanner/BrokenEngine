<!-- broken-engine-plan/v2 {"createdUtc":"2026-11-26T14:01:19.037Z","dependsOn":[]} -->
# Implementer comments use code identifiers, and C++/GLSL briefs cite the style guide

## Context

`.agents/skills/implement-plan/references/worker.md` and
`.agents/skills/resolve-findings/references/worker.md` never mention code
comments or `Documents/C++StyleGuide.txt` rule 64 (no Grep hit for "comment"
or "rule 64" in either). `.agents/skills/implement-plan/SKILL.md` `## Inputs`
does not require the style guide in a C++ or GLSL brief's `Governing paths`.
Plan and main-session wording leaked into code comments:

- `Engine/Data/Shaders/Lighting/LightingSpread.frag:35` (landed in
  `7561b503b`): the implementer wrote "Unthresholded Reinhard compression", a
  word taken from the plan text. Rule 64 bans "a comparison against replaced
  code".
- `Engine/Data/Shaders/Objects/HexShield.vert:91` (landed in `81c17f92c`):
  the implementer replaced
  "refreshing lighting layer's flight plane" with "base flight plane", a term
  main coined in its plan presentation; `git grep` at `7561b503b` finds no
  earlier use. The term also reached
  `Engine/Source/Graphics/Render/AGENTS.md:36`. Root `AGENTS.md` "One term per
  concept" forbids coined nicknames.
- The shader implementer briefs of both sessions listed root `AGENTS.md` but
  not `Documents/C++StyleGuide.txt` in `Governing paths`.

## Design

- `implement-plan/references/worker.md` `## Rules`: one rule — comments follow
  `Documents/C++StyleGuide.txt` rule 64 and name concepts by their code
  identifier or the established repository term, never by plan or brief
  wording.
- `implement-plan/SKILL.md` `## Inputs`: when the assigned items change C++ or
  GLSL, `Governing paths` includes `Documents/C++StyleGuide.txt`.

The author recommends citing rule 64 and the root "One term per concept"
directive by name rather than restating them. This ranks lowest of the
comment-gap fixes because it depends on compliance; the scanner and review
class changes carry the mechanical part.

## Critical files

- `.agents/skills/implement-plan/references/worker.md`
- `.agents/skills/implement-plan/SKILL.md`

## In scope

- `implement-plan/references/worker.md` `## Rules`: the comment rule.
- `implement-plan/SKILL.md` `## Inputs`: the Governing-paths requirement.

## Out of scope

- `Documents/C++StyleGuide.txt` rule 64 wording and
  `.agents/skills/comment-review` classes (another change owns them).
- `.agents/skills/resolve-findings` (another change edits it).
- Rewording the existing shader comments named in `## Context`.

## Acceptance criteria

- The worker rule and the `## Inputs` requirement exist and cite rule 64 and
  the style guide by path.

## Notes

- Change Workflow tier: Tier 2 (scoped workflow behavior: one skill).
- Run `/external-skill-creator` validation after the skill package change.
