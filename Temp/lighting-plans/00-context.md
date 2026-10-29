# Lighting foundation: shared context for plans 01-05

## Current user correction: minimum/default zoom

The user replaced the excessive 6000 m fixture with actual gameplay minimum/default zoom.
Current scope is that correction, targeted builds and a short fresh-binary smoke check of both
presets, 1920x1080 restore, patched save load and readiness. No exhaustive benchmark campaign
or prototype algorithm work is authorized. Keep kiTilesPerFrame=120 and the memory disposition.
The old run reached 54960 tiles at 905.68 s, then the user canceled; all resources were released
and logs had no errors. That is historical incomplete evidence, not a completed 15-minute test.
Exports/radiometry and the old exhaustive acceptance rows remain incomplete. Keep plan03 unchecked.
Row3 retains its ordinary-gameplay comparison fallback PASS; new presets receive smoke checks only.
Bounded completion: Temp/foundation-zoom-results.md ## Bounded completion records the
minimum/default correction builds and short smoke as verified; exhaustive Plan 03 remains
unchecked under ## Progress.

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
- The current foundation diff baseline is `cd6d410c064a5a5f905bf68efaf28493ab6fc33a`.
  The user's rebase-from-main instruction replaces the prior fixed baseline
  `b56f20fa44d0424c2fdf5003157563684ed747f5` for current diffs;
  historical evidence keeps its inputs. This is the pinned user-request target; main later
  advanced independently to `40b82adfe90b447ee848120b4a26bad546b83a49`. Do not chase it.
- Rebased source checkpoint: `1c3608533ba58d1a1327b51928ed4dced5426c53`.
  Prior tip `d170b18b843a3a96af227cef467b824b4aa214c5` remains at safety ref
  `safety/lighting-foundation-pre-rebase-20261029`. Rebase decisions and commit map:
  `Temp/lighting-rebase-evidence.md` `## Conflicts and resolutions` and `## Commit map`.
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
  `kfLightingRayOffset = 0.05f` (5 cm); traced terrain need only agree within it;
  no exact-equivalence proofs.
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

Historical strip checkpoint: no build then (the pre-strip tree built cleanly — envelope
`Temp/AgentBuildEnvelopes/compile-20261029T115329564Z-21120.md`), no Step 6 reviews, no runtime
acceptance.

## Workflow

Change Workflow (`.agents/references/change-workflow.md`), Tier 3. Main manages; subagents do the
work with the roles the workflow names. Briefs follow `.agents/references/subagent-reporting.md`
`## Task brief`; take identity values from `Get-AgentWorktreeSessionContext`.

## Progress

- Rebase from pinned main completed at the Identity checkpoint, followed by review corrections.
  DataPacker Release x64 PASS remains current in
  `Temp/AgentBuildEnvelopes/compile-20261029T190055928Z-45844.md`
  `## Rebased lighting foundation builds`. Latest client/server/ModelViewer Debug x64 builds
  PASS with zero diagnostics after the readiness correction; all three used Local generation and
  `RunDataPacker=true`. Evidence:
  `Temp/AgentBuildEnvelopes/compile-20261029T204958125Z-36232.md`
  `## Plan03 readiness build results`. Earlier build/runtime results below are historical.
  Current row2 exact-pattern searches both returned no matches; row9 static examination PASS.
  The user authorized the save-file header254-to-256 update; byte and backup proof:
  `Temp/plan03-save-update.md` `## Recorded byte verification`.
  The matched-reference approval below supersedes row3 historical setup.
  Conflict propagation/docs evidence: `Temp/rebase-propagation.md`.
  Rebase verification index: `Temp/lighting-plans/rebase-20261029.md` `## Verification`.
  C++, shader, style/membership, DOC1 and comment reviews pass. Adversarial ADV001 was fixed;
  fresh independent C++/adversarial retest passes. Final rebase static checks pass:
  InvokeStaticChecks on cd6d410c passed 4 checks (2 packages, 93 links, 227 hygiene files,
  78 line-length files). Final acceptance review PASS: criteria 8/8, findings none. The index
  records each scoped result. The bounded camera/smoke request is complete;
  exhaustive plan03 remains incomplete.
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
- [x] Latest bounded user request: minimum/default common-foundation camera and short smoke.
  Fresh client/server/ModelViewer Debug x64 Local builds PASS; short smoke PASS6/6 in81s.
  No pending build/smoke, prototype algorithm or throughput tuning. Completion evidence:
  Temp/foundation-zoom-results.md ## Bounded completion.
  Final static and fresh bounded acceptance review remain manager-owned.
- [ ] 03 Original exhaustive runtime matrix (incomplete; not the current bounded request).
  Rows1/2 and row9 static PASS; attempted-run logs clean. Row3 fallback PASS remains recorded:
  exact cd6d410c Lighting=false reference matched save/identity, sun1.8/FoV15 and readiness.
  Numeric over8 failed: first close/wide38.343%/19.895%, retake39.650%/19.892%.
  No added or missing local glow was observed; no pixel-match pass is claimed. Evidence:
  Temp/lighting-matched-reference/reference-off-runtime-evidence.md
  ## Matched readbacks and images, ## Row3 comparison and visual verdict, ## Logs and release.
  Historical captures did not complete exports/radiometry. The canceled6000m capture exceeded
  its old deadline; keep that failure as history, not as a current bounded-scope failure.
  Retain the memory document/link from historical wide-view GPU evidence; no normal-preset
  performance conclusion is claimed. History: Temp/foundation-zoom-results.md
  ## Historical limits and next action. All owned resources released.
  Do not restart long captures or benchmarks. Wait for user direction on future validation
  or the next plan; plans04/05 were not executed or claimed.
- [ ] 04 Commit and create the `lighting-foundation` branch
- [ ] 05 Stage 2 one-bounce reference (prepare only)
