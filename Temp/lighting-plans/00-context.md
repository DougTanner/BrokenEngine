# Lighting foundation: shared context for plans 01-05

Every plan in `Temp/lighting-plans/` reads this file first. Run the plans in number order, one per
fresh session; each ends by updating `## Progress` below.

## Identity

- Use the existing lighting worktree
  `C:/Users/dougt/.claude/worktrees/BrokenEngine/0cb8b7b1-9695-4b87-ab78-8bf06eceacad`
  on branch `lighting-foundation-work`; do not create another worktree. Session steps:
  `CODEX-HANDOFF.md` in this folder.
- The user's latest correction supersedes the earlier primary-checkout instructions.
  Primary `C:/Users/dougt/Documents/BrokenEngine` is clean on `main` and available to
  another agent; do not switch or modify it.
- The fixed foundation diff baseline is `b56f20fa44d0424c2fdf5003157563684ed747f5`,
  even if main advances. Use this baseline in place of older diff-base instructions.
- Current checkpoint: `b01f2286` saves source changes and review evidence through the
  partial plan 02 work; remaining verification is recorded in `## Progress`.
- The snapshot was authored on main `aa47e4a3e170a69b7dad971b94003f7ea30e5716`.
  Snapshot history: foundation WIP, memory-cost experiment, handoff doc, revert of the
  experiment, then the strip-stage snapshot.
- The strip-stage snapshot was pushed as `lighting-foundation-handoff-20261029`.
  It force-adds this plan folder and the cited `Temp/` evidence (gitignored working notes,
  not `/next-plan` Plans); plan 04 removes them from the index before the foundation commit.
  Keep these notes in `Temp/`, not `Documents/Plans/`.
- Origin keeps the untouched pre-strip snapshot on `lighting-foundation-handoff-20261028`.

## Goal and authority

- User's original request: "The following plans will need some code in common, create the common
  denominator they will need, ex: the old lighting system is being completely replaced, so that
  should be stripped out and only stubs left." The result is a reusable `lighting-foundation`
  branch next to main; main stays unchanged.
- Consumers: `Documents/Investigations/Engine/LightingPrototype{Clustered,DDGI,DirectionalCache,
  ReGIRReSTIR,SplitRadianceCascades}.md`.
- Authoritative completion list: `Documents/Investigations/Engine/LightingFoundationHandoff.md`
  `## Acceptance table` (9 rows). Its `## Decisions`, `## Known omissions` and
  `## Stage 2: one-bounce indirect reference` sections bind.
- The old L1-L14 list and the old Codex `Temp/` evidence are history only; do not reopen them.

## User decisions (binding; do not re-ask)

- "Just strip out things that are over-engineered or not useful for a possibly-discarded quick
  prototype, no need for my approval." Main decides strip and review-finding questions; YAGNI wins.
- Terrain accuracy: "way overkill, definitely Option 2 or even looser" — shared ray start offset
  `kfLightingRayOffset = 0.05f` (5 cm); traced terrain need only agree within it; no exact-equivalence
  proofs.
- One-bounce indirect reference is a second foundation stage (plan 05), not part of this stage.
- The memory-cost experiment stays reverted; its question is
  `Documents/Investigations/Engine/LightingTerrainPreparationMemoryCost.md` (settled in plan 03).
- The user-selected RAM placement / OOM fallback policy in
  `Engine/Source/Graphics/Objects/Buffer.cpp` stays.
- The user clears the session between plans; keep each plan's work inside its plan.

## What the strip stage did (uncommitted)

Plan: `Temp/lighting-foundation-strip-plan.md` (keep/strip table, capture design, public interface,
acceptance table). Main also accepted, beyond the plan: removing the unused `vkGeometryAddress`
parameter of `LightingTerrain::Write`.

- Deleted diagnostics: terrain validation/witness shaders, scene diagnostics, manifests, hashes,
  byte/operation accounting, RenderDoc helpers, about 95 TEMP blocks (production lines hidden in
  TEMP blocks were restored to `aa47e4a3` text).
- Terrain traced only from the visible elevation texture; off-screen vertices at sea floor.
- Terrain GPU time: ProfileManager timer `kGpuTimerLightingTerrain`.
- Capture: progressive under client pause (status in `Lighting::State`; fails if the published
  scene changes); results in `capture.json`.
- Project files reconciled. `.agents/skills/update-vcxproj/scripts/Update-EngineVcxprojItem.ps1:301`
  got a one-line ModelViewer exclusion (main has the same bug; see plan 04).
- Documents rewritten (handoff, memory-cost doc, five prototype docs, AGENTS.md files,
  `Documents/Architecture/FrameUpdatePipeline.md`, agent-harness command reference).
- `/code-style-review` applied 25 fixes (`Temp/temp-style-review-detail.md`).

Not yet done: no build since the strip (the pre-strip tree built cleanly — envelope
`Temp/AgentBuildEnvelopes/compile-20261029T115329564Z-21120.md`), no Step 6 reviews, no runtime
acceptance.

## Workflow

Change Workflow (`.agents/references/change-workflow.md`), Tier 3. Main manages; subagents do the
work with the roles the workflow names. Briefs follow `.agents/references/subagent-reporting.md`
`## Task brief`; take identity values from `Get-AgentWorktreeSessionContext`.

## Progress

- [x] Strip stage implemented (this session).
- [x] 01 Build and static checks.
  DataPacker Release x64 and client/server/ModelViewer Debug x64 succeeded with zero diagnostics;
  all three consumers used Local generation with `RunDataPacker=true`.
  Build evidence: `Temp/AgentBuildEnvelopes/compile-20261029T135736358Z-8248.md`
  `## Build envelopes`.
  All static checks pass after whitespace-only rewrap of eight lines across
  `Temp/lighting-foundation-strip-plan.md`, `Temp/temp-slice2-handoff.md`, and
  `Temp/temp-style-review-detail.md`; no source fixes were needed.
  Setup merge `b50c0621` resolved seven conflicts: render docs match the merged snapshot;
  client/server projects retain main ModelMetadata plus foundation headers; ModelViewer retains
  main MenuUtils and removes deleted LightingWrappersBase.
  Reviews and runtime acceptance remain plans 02/03; no plan 01 residuals.
- [x] 02 Reviews and cleanup.
  Seven overengineering removals proved; shared TerrainVertexCoordinates extracted and
  retested. Full C++ review covered 102 paths; GLSL review covered 51 paths.
  CXX001-004, naming, coherence C1-3 and comments F1-3 resolved and retested.
  ADV001/CXX005 capture lifecycle and CXX006 single-predicate correction independently pass;
  ADV002 obsolete probe schema corrected and skill retest passes. Final style, comments,
  propagation/docs, membership, skill-package, progressive-disclosure and coherence checks pass.
  Rejected: converting Lighting to class/merging its .cpp files; struct/private coupled state
  is valid and the implementation split is exempt.
  Rejected: renaming historical plan/review snapshot identifiers; they record original evidence,
  while live code/docs use the current names.
  DataPacker Release x64 succeeded in this worktree:
  `Temp/AgentBuildEnvelopes/compile-20261029T150823266Z-41536.md` `## Build envelopes`.
  Latest client/server/ModelViewer Debug x64 succeeded with zero diagnostics, Local generation
  and `RunDataPacker=true`:
  `Temp/AgentBuildEnvelopes/compile-20261029T154650516Z-22496.md` `## Build envelopes`.
  Static checks pass; no confirmed residuals. Runtime acceptance was not run (plan 03).
  Dispositions and evidence index: `Temp/lighting-plans/02-review-results.md` `## Review results`.
- [ ] 03 Runtime acceptance
- [ ] 04 Commit and create the `lighting-foundation` branch
- [ ] 05 Stage 2 one-bounce reference (prepare only)
