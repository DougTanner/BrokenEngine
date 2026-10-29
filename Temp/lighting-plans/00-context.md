# Lighting foundation: shared context for plans 01-05

Every plan in `Temp/lighting-plans/` reads this file first. Run the plans in number order, one per
fresh session; each ends by updating `## Progress` below.

## Identity

- Work in your own wrapper session worktree and branch (setup: `CODEX-HANDOFF.md` in this folder).
  Identity values (worktree, branch, baseline) always come from `Get-AgentWorktreeSessionContext`.
- The snapshot was authored on main `aa47e4a3e170a69b7dad971b94003f7ea30e5716`. Where a plan says
  `aa47e4a3…` as the diff base, use your session baseline if main has moved since (the foundation
  change is always session baseline → HEAD).
- Snapshot history on top of `aa47e4a3`: foundation WIP, memory-cost experiment, handoff doc,
  revert of the experiment, then the strip-stage snapshot.
- The strip stage (below) is committed as a WIP snapshot on the session branch and pushed to origin
  as `lighting-foundation-handoff-20261029`. That commit force-adds this plan folder and the `Temp/`
  files the plans cite (gitignored working notes, not `/next-plan` Plans); plan 04 removes them from
  the index before the foundation commit.
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
- [ ] 01 Build and static checks
- [ ] 02 Reviews and cleanup
- [ ] 03 Runtime acceptance
- [ ] 04 Commit and create the `lighting-foundation` branch
- [ ] 05 Stage 2 one-bounce reference (prepare only)
