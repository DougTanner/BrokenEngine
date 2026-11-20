<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-19T22:17:18.879Z","dependsOn":[]} -->
# Name the game for a ModelViewer build in /compile

## Context
`/compile` uses "game build" for two different sets. `.agents/skills/compile/SKILL.md:54-58`
ties the game project to a client or server build and says ModelViewer builds
"always use BrokenEngineSandbox as their data owner; they do not add another
game project", while `.agents/skills/compile/scripts/Invoke-CompileBuild.ps1:49`
lists ModelViewer among `$script:GameTargets` (with Client, Server, and
ClientServer) and `.agents/skills/compile/references/runtime-data-mode.md:116-117`
runs the same data-mode resolution for it. The `## Handoff` row at
`SKILL.md:103-105` asks, "for a game build", for one row "naming the game" and
the envelope's `/p:DataBuildMode`, `/p:RunDataPacker`, and
`/p:GameDataDirectory` values, while the success row at `:96-99` and the
failing-build row at `:100-102` name a game only "for a client or server build".
So whether a ModelViewer build gets that row, and which game it names, is
unstated; the evident reading is BrokenEngineSandbox, its data owner.

## Design
Reword the `SKILL.md` `## Handoff` game-build row so it covers ModelViewer
explicitly and names BrokenEngineSandbox, the data owner, as its game.
`.agents/skills/compile/references/worker.md:293-294` refers to the same row
("row for a game build that names the envelope's switch values"); keep it
parallel.

## Critical files
- `.agents/skills/compile/SKILL.md`
- `.agents/skills/compile/references/worker.md`

## In scope
- The game-build `Decisive checks` row in `SKILL.md` `## Handoff` and its
  counterpart in `references/worker.md`.

## Out of scope
- `Invoke-CompileBuild.ps1` and every script.
- The ModelViewer data-owner rule itself.

## Risk tier and invariants
Tier 1 — documentation wording in one skill. A `compile` skill package change
takes `/external-skill-creator` validation.

## Acceptance criteria
- The row states that a ModelViewer build reports BrokenEngineSandbox as its
  game, and `SKILL.md`, `references/worker.md`, and `runtime-data-mode.md`
  agree.
