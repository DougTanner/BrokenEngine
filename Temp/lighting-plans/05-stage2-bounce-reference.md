# 05 Stage 2: one-bounce indirect reference (prepare only)

Read `Temp/lighting-plans/00-context.md` first. Requires plan 04 done (`lighting-foundation`
exists). The user chose to add this as a second foundation stage; this plan prepares it and stops
for the user's approval before implementation.

## Objective

Draft a decision-complete plan for a shared one-bounce diffuse indirect reference on top of
`lighting-foundation`, so DDGI and Split Radiance Cascades compare against the same reference.

## Inputs

- `Documents/Investigations/Engine/LightingFoundationHandoff.md`
  `## Stage 2: one-bounce indirect reference`.
- `Documents/Investigations/Engine/LightingPrototypeDDGI.md` and
  `LightingPrototypeSplitRadianceCascades.md` — their indirect reference and colored-wall bounce
  requirements.
- `Engine/Data/Shaders/Lighting/LightingQuery.h` `LightingClosestHit` (hit material/albedo) — never
  verified; stage 2 is its first consumer and must verify it.

## Constraints

- Brute force at 1000 lights is impractical (about 10^16 shadow rays per 1920x1080 image at 256
  bounce rays), so the plan must pick a feasible reference scene: fewer lights, lower resolution,
  fewer bounce rays, or a cached direct-irradiance pass at hit points. Present the choice to the
  user with options and a recommendation; the same YAGNI rule applies (possibly-discarded
  prototypes).
- Reuse the stage-1 capture, fixture and export; no new harness commands; no unit tests.

## Steps

1. Work in a fresh wrapper worktree branched from `lighting-foundation` (ask the user how they want
   it created; do not reuse this worktree once plan 04 is done unless the user says so).
2. `implementer` `/prepare-change`; then `/plan-alternatives`, `/plan-audit`,
   `/plan-simplicity-review`, and `/external-grill-plan` per the Change Workflow (Tier 3).
3. Present the plan to the user for approval. Stop.
