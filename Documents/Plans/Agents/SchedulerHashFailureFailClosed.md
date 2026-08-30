<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-30T21:25:01.981Z","dependsOn":[]} -->
# Fail closed when scheduler key hashing is unavailable

## Context

`coordination::HashSha256` returns `std::nullopt` when BCrypt setup, hashing,
or finalization fails (`Tools/ToolCommon/CoordinationStore.cpp:141-183`).
`MakeLocator` already preserves that failure (`CoordinationStore.cpp:271-284`),
but `SchedulerRoot` and `ClaimPath` turn it into the literal `invalid` and
`invalid.json` (`Tools/WorktreeCli/PlanScheduler.cpp:214-228`).  The scheduler
therefore can alias repositories or Plans after a provider failure, acquire a
guard in the wrong namespace, heal a peer record, or publish a claim under a
fallback path.  `HealClaims` can delete records while checking the derived
filename (`PlanScheduler.cpp:286-324`), and the affected root/path helpers are
reached by non-lint `validate`, `list`, `claim-next`, `claim-status`,
`unclaim`, `complete`, and `reject` (`PlanScheduler.cpp:495-550,590-650,
692-804,815-1033`).

The originating gap is the missing fail-closed propagation and deterministic
failure-isolation evidence.  `validate --lint-only` completes before scheduler
storage and remains storage-free.  No BCrypt incident was observed; this is a
source-proven, ultra-rare failure hypothetical.  The current tree has no
deterministic hash-failure seam, and no live Plan owns this root cause and
implementation boundary.

## Design

The author's recommendation is checked optional propagation because it keeps
successful hashing and the existing coordination locator unchanged while
preventing a failed hash from becoming a filesystem path:

- Keep successful `HashSha256` and `MakeLocator` behavior unchanged.  Remove
  the scheduler's `invalid` and `invalid.json` fallbacks.  Keep
  `SchedulerRoot` optional and make `ClaimPath` optional.
- Check `SchedulerRoot` in every storage-using verb.  A missing repository
  hash stops all seven verbs — `validate`, `list`, `claim-next`,
  `claim-status`, `unclaim`, `complete`, and `reject` — before scheduler-root,
  guard, or claim-storage access.
- Check `ClaimPath` in its current consumers: non-lint `validate` through
  `HealClaims`, `list`, and `claim-next`.  `HealClaims` should first collect
  and preflight all removals that need a derived Plan filename; if any
  required Plan hash is unavailable, it returns failure before any
  `DeleteFileW`, so no earlier deletion is applied.
- Do not add Plan hashing to `claim-status`, `unclaim`, `complete`, or
  `reject`; their repository-keyed claim scan remains the current operation.
  Keep `validate --lint-only` unchanged.

Repository or Plan hash unavailability uses the existing `Failure` envelope:
`schemaVersion: 2`, `status: "error"`, `code: "local-app-data-unavailable"`,
and exit code `1`.  No new error code, schema, fallback namespace, or storage
mutation is introduced.

The user-selected deterministic seam is a guarded exact-input environment
seam inside shared `HashSha256`.  Only the exact pair
`BROKEN_ENGINE_COORDINATION_HASH_FAILURE_FIXTURE=plan-scheduler-v1` and
`BROKEN_ENGINE_COORDINATION_HASH_FAILURE_INPUT=<the exact current UTF-8
input>` returns `std::nullopt` before BCrypt.  An absent, incomplete, invalid,
or nonmatching pair follows the current BCrypt path.  The fixture sets and
restores both variables around each child invocation.  There is no CLI flag,
new executable, callback API, persistent state, compatibility path, or
alternate hash.

The existing scheduler fixture should invoke the real candidate
`WorktreeCli.exe`.  It should derive the canonical Git-common-directory input
and one normalized Plan path independently, then exercise repository-hash
failure across all seven storage verbs and Plan-hash failure across
non-lint `validate`, `list`, and `claim-next`.  Each forced case should assert
the fixed envelope, byte-identical isolated scheduler state, no `invalid`
root or claim file, no wrong-store mutation or peer-record deletion, and no
claim publication.  The ordinary scheduler scenarios remain part of the
passing suite.  Because the seam is in shared `ToolCommon`, the future build
covers both WorktreeCli and AgentHarness.

## Critical files

- `Tools/ToolCommon/CoordinationStore.cpp:141-183,271-284` — shared hash and
  locator behavior, including the guarded fixture seam.
- `Tools/ToolCommon/CoordinationStore.h:46` — shared `HashSha256` contract.
- `Tools/WorktreeCli/PlanScheduler.cpp:214-228,286-324,495-550,590-650,
  692-804,815-1033` — scheduler root/path propagation, healing, and all
  storage-using command callers.
- `Tools/WorktreeCli/AGENTS.md:18,24-28` — repository-keyed scheduler,
  fail-closed storage, and healing contract.
- `.agents/scripts/Test-WorktreeCliPlanScheduler.ps1` — existing real-CLI
  fixture and isolated scheduler-state assertions to extend.

## In scope

- Add the guarded, exact-input environment seam inside shared `HashSha256`
  with the user-selected pair and unchanged BCrypt path for every other
  input.
- Propagate optional repository and Plan paths through the named scheduler
  callers, removing both scheduler fallback strings and preserving the
  existing success behavior and error envelope.
- Make `HealClaims` preflight required Plan-derived filenames and apply no
  deletion when that preflight cannot produce a hash.
- Extend the existing scheduler fixture to run the real candidate WorktreeCli
  with independent canonical repository and normalized Plan failure inputs,
  covering the seven repository-failure verbs and three Plan-failure
  consumers, including byte/state, no-alias, no-publication, and no-deletion
  assertions.
- Build and check both WorktreeCli and AgentHarness because they compile the
  shared ToolCommon change, then run the existing scheduler fixture and the
  applicable Change Workflow reviews and documentation synchronization.

## Out of scope

- `PlanMetadata.cpp`'s unused digest fallback.
- The BCrypt algorithm or digest implementation, and MakeLocator or
  landing-lock call sites beyond the shared seam's compile exposure.
- Claim schema, selection, expiry, orphan-healing policy, metadata format, or
  unrelated scheduler behavior.
- A new fixture executable or project, public CLI switch, callback API,
  persistent state, compatibility path, alternate hash, or unit tests.
- Any game/runtime determinism or CRC, replay, wire, serialization, `.pack`,
  graphics, shader, frame-layout, or simulation change.
- Source implementation during this Plan-recording stage.

## Risk tier and invariants

This Plan authoring stage is Change Workflow Tier 1 documentation. Executing
the Plan is Tier 3 because it changes a shared tool's cross-session
coordination trust boundary and can affect other sessions' claims.

- A repository hash failure cannot create or inspect a scheduler root, guard,
  claim directory, or fallback namespace.
- A Plan hash failure cannot produce an unavailable claim path, publish a
  claim, delete a claim, delete a peer record, or mutate an earlier healing
  target; `HealClaims` aborts before deletion.
- Successful hashing keeps the existing Git-common-directory and normalized-
  Plan namespaces, claim schema, selection, expiry, and healing behavior.
- `claim-status`, `unclaim`, `complete`, and `reject` do not gain Plan
  hashing, while non-lint `validate`, `list`, and `claim-next` check the
  optional Plan path before using it.
- `validate --lint-only` remains storage-free and does not create, read, heal,
  or delete scheduler state.
- The seam is opt-in only for the exact fixture pair and exact input; all
  other inputs use the current BCrypt path.

## Coordination

This Plan has no dependencies. The future shared ToolCommon change requires
WorktreeCli and AgentHarness builds, but it adds no project or scheduler
dependency and leaves the isolated fixture's environment restored after every
child invocation.

## Acceptance criteria

- With the fixture pair absent, incomplete, invalid, or nonmatching, normal
  BCrypt success and all existing scheduler success behavior remain
  unchanged.
- For the exact canonical repository input, each of `validate`, `list`,
  `claim-next`, `claim-status`, `unclaim`, `complete`, and `reject` returns
  the existing v2 `local-app-data-unavailable` error envelope with exit `1`
  before root/guard/storage access.  The isolated scheduler state remains
  byte-identical and contains no `invalid` root, guard, claim file, or
  publication.
- For the exact normalized Plan input, non-lint `validate`, `list`, and
  `claim-next` return that same envelope before using the unavailable
  `ClaimPath`; no claim is published, no wrong namespace is read, and
  `HealClaims` deletes nothing, including records it had already classified.
- `validate --lint-only` remains storage-free with no healed claims, and the
  pre-existing scheduler fixture scenarios continue to pass alongside the
  new failure-isolation cases.
- The future WorktreeCli and AgentHarness builds pass, and the fixture is run
  against the real candidate WorktreeCli via
  `pwsh -NoProfile -File
  .agents/scripts/Test-WorktreeCliPlanScheduler.ps1
  -WorktreeCliExecutable <candidate path>`.

## Notes

- This is one independently executable Plan with no `dependsOn` edge.  It
  records the user's selected environment seam after the failure scenario and
  alternatives were disclosed; all design choices are settled.
- The current compact scheduler check before authoring was valid/ok with 95
  executable Plans, no diagnostics or notices, and no healed claims.  This
  stage intentionally performs no source implementation or build.
- Future execution must receive the Tier-3 plan-audit, plan-simplicity,
  external-grill, coherence, scope, adversarial, build, fixture,
  progressive-disclosure, and verification coverage required by the Change
  Workflow before landing.
