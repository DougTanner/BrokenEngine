<!-- broken-engine-plan/v2 {"createdUtc":"2026-11-26T14:01:13.027Z","dependsOn":[]} -->
# Scope review checks the plan's Design-section behavior promises

## Context

`.agents/references/scope-authorization.md` step 1 (authorization pass) maps
each changed region to the `## In scope` entry or user instruction that
authorizes it, and flags only an unmapped region or one matching an
`## Out of scope` entry. Nothing in the three passes compares a changed region
against a behavior the plan's `## Design` section promises to keep. A change
inside an In-scope function that breaks such a promise therefore passes.

Observed instance (squash `7561b503b`, which completed Plan
`SingleFlightHeightShips.md`, readable at
`git show 7561b503b^:Documents/Plans/Game/SingleFlightHeightShips.md`):

- The Plan's `## Design` terrain-response table (lines 95-98) promises
  "Live Battleship | Steers away and bounces on contact, as today" and maps
  dying Fighters and live Battleships to `ApplyTerrainBounce`.
- Its `## In scope` (line 257) lists `ApplyTerrainBounce` only for dropping its
  plane argument.
- `/plan-audit` finding PA-F-006, accepted into the plan snapshot, read
  "Reflect only inward motion for live Omnis."
- The implementation put the inward-only guard into the shared
  `ApplyTerrainBounce`
  (`Projects/BrokenEngineSandbox/Source/Frame/Collections/Ships/ShipBehaviours.cpp:449-452`,
  the `XMVector3Dot(rVecVelocity, result.vecNormal) < 0.0f` test), which
  Battleships (`Battleships.cpp:501`) and dying Fighters (`Fighters.cpp:671`)
  also call. Their bounce changed against the "as today" promise.
- The scope review passed it because `ApplyTerrainBounce` mapped to an
  In-scope entry. The plan-simplicity reviewer noted "Guarding the shared
  reflection" without flagging the wider reach, and the landing message said
  only "updates live Omni horizontal terrain push and bounce".

## Design

The scope review's authorization pass also checks each changed region against
the behavior promises the plan's `## Design` states (rows or sentences that say
a behavior stays as it is, or bound a change to a named case such as "for live
Omnis"), and reports a region that changes a promised behavior as
`unauthorized`, citing the promise.

This is one edit in the file every reviewer already applies
(`coherence-review`, `glsl-review`, `over-engineering-review`,
`progressive-disclosure-review`, and `repo-code-review` cite it), it covers
Plans already written, and it does not depend on each plan author copying every
promise twice.

Rejected alternative: Plans restate each behavior promise as an
`## Out of scope` entry (the `/prepare-change` drafting rule and the Plan shape
in `Documents/Plans/AGENTS.md` `## Plan files`), so the existing Out-of-scope
match catches it. It keeps the review mechanical but leaves the gap open
whenever a promise is not restated, duplicates Design facts in Out of scope
against the progressive-disclosure directive, and edits files outside
`## Critical files`.

## Critical files

- `.agents/references/scope-authorization.md`

## In scope

- `.agents/references/scope-authorization.md`: the authorization source
  paragraph and step 1 (authorization pass), to add the Design-promise check.

## Out of scope

- Changing the Battleship and dying Fighter bounce in `ApplyTerrainBounce`;
  that game behavior is a separate decision.
- The reviewer skills that cite `scope-authorization.md`; they keep citing it
  unchanged.
- `.agents/skills/repo-code-review`, `.agents/skills/resolve-findings`, and
  `.agents/skills/comment-review` content other changes own.

## Acceptance criteria

- Applying the revised pass to the `SingleFlightHeightShips.md` Plan text and
  the `ApplyTerrainBounce` inward-only guard yields an `unauthorized` finding
  citing the Design-table "as today" Battleship promise.
- A region that maps to an In-scope entry and breaks no stated Design promise
  still reports `Scope: PASS`.

## Notes

- Change Workflow tier: Tier 2 (scoped tool behavior: one workflow
  reference's review rule). No determinism, wire, save, or build exposure.
