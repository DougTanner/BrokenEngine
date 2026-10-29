# 01 Build and static checks

Read `Temp/lighting-plans/00-context.md` first.

## Objective

Get the stripped foundation compiling cleanly with regenerated shader data, and pass the static
checks (Change Workflow Step 5).

## Steps

1. `builder` runs `/compile`: DataPacker Release x64 (Local shader data — shaders were deleted and
   changed), BrokenEngineSandbox client and server Debug x64, ModelViewer Debug x64. Agent-harness
   trigger: yes (plan 03 runs the harness).
2. For every compile, link or shader error: a separate `implementer` runs `/resolve-findings` with
   the error as fixed evidence, scope limited to the failing site and its owning slice's intent
   (`Temp/lighting-foundation-strip-plan.md` `## Public interface after the strip` is the contract).
   Likely spots named by the slices: `kVkExtent` rename and the DeviceManager `insert` from the
   style pass; `Lighting.h` dropped its `struct FrameInterpolateBase;` forward declaration;
   GLSL binding 5 layout in `LightingTerrain.comp`. If a fix changes C++, re-run
   `/code-style-review` (mechanic) over the newly changed ranges before rebuilding. Repeat 1-2 until
   all four targets pass.
3. Main runs `pwsh -NoProfile -File .agents/scripts/Invoke-StaticChecks.ps1 -RepositoryRoot
   '<worktree root>' -Baseline aa47e4a3e170a69b7dad971b94003f7ea30e5716 -IncludeUntracked`; an
   `implementer` rewraps every `line-length` failure and fixes other failures.

## Acceptance

- All four targets build with zero errors; envelope path recorded.
- Static checks status `pass`.

## Out of scope

Reviews (plan 02), harness runs (plan 03), commits (plan 04). Do not re-add stripped code to make
something compile; if a deletion was wrong, restore only the missing production behavior and note
it in `## Progress`.

## Finish

Tick 01 in `00-context.md` `## Progress` with the build envelope path and a one-line note of any
fix.
