<!-- broken-engine-plan/v1 {"createdUtc":"2026-09-27T16:16:36.852Z","dependsOn":[]} -->
# Fix: Recover AudioManager endpoint HRESULT failures without exceptions

## Context

`Documents/C++StyleGuide.txt:68` (rule 9) reserves exceptions for fatal
errors. `AudioManager::AudioManager` (`Engine/Source/Audio/AudioManager.cpp:270-300`)
wraps endpoint discovery and subsystem setup in a try whose
`catch (const std::exception&)` (:290) and `catch (...)` (:295) arms log
"Failed to create AudioManager" and return, so the client runs without audio.
The first-party throws they recover are `CHECK_HRESULT` failures
(`Common/ErrorUtils.h:21`, which throws `std::runtime_error` in every
configuration via `common::CheckHresult`, `Common/ErrorUtils.cpp:24-33`) in
`GetEndpointId` (:28), `InitializeAudioEndpoint` (:44, :68, :76, :85, :110)
and `ConfigureLiveGraph` (:241). That is a non-fatal recovery carried by an
exception, a rule 9 finding under
`.agents/skills/repo-code-review/references/checks.md` `### Style guide contracts`.
A constructor has no return value, so no existing non-throwing result channel
can replace the throw; the rule sweep Plan that found it left it out of scope.

The same try also covers `std::make_unique<AudioEngine>` (:38, :137), a
DirectXTK constructor that can throw its own exception. Catching a third-party
throw is a permitted form of rule 9 and is not this finding.

## Design

Author's recommendation: reuse the silent-engine fallback that
`InitializeAudioEndpoint` already has (:130-138, "Always yield one stable
engine"). Replace each `CHECK_HRESULT` in `GetEndpointId`,
`InitializeAudioEndpoint` and `ConfigureLiveGraph` with a plain HRESULT test
that logs the failure through `common::HresultToString`
(`Common/WindowsUtils.h:8`) and takes the matching existing degraded path:

- `GetEndpointId` returns an empty id, which callers already treat as
  "skip this device" (:87-91, :112-121).
- A failed `CoCreateInstance` leaves no enumerator to query (:49, :68
  dereference it), so it skips default-endpoint lookup and enumeration and goes
  straight to the OS-default silent engine (:134-138).
- A failed `EnumAudioEndpoints` leaves the device collection null, which
  already falls through to the OS-default silent engine (:125-128, :134-138).
- A failed `GetCount` leaves `uiCount` 0; a failed `Item` skips that device.
- A failed `GetChannelMask` in `ConfigureLiveGraph` skips only the
  diagnostic log.

Then narrow the constructor's try to the DirectXTK `AudioEngine` constructions
it still needs to cover, or keep the constructor try with only the
`std::exception` arm when that is the smaller diff; remove the `catch (...)`
arm, which has no remaining first-party source. Behavior when every HRESULT
succeeds is unchanged.

## Critical files

- `Engine/Source/Audio/AudioManager.cpp:25-139` — `GetEndpointId`,
  `CreateAudioEngineForEndpoint`, `InitializeAudioEndpoint`.
- `Engine/Source/Audio/AudioManager.cpp:168-255` — `ConfigureLiveGraph`
  (the `CHECK_HRESULT` at :241).
- `Engine/Source/Audio/AudioManager.cpp:270-300` — the constructor try/catch.
- `Common/ErrorUtils.h:21`, `Common/ErrorUtils.cpp:24-33` — why
  `CHECK_HRESULT` throws.

## In scope

- The `CHECK_HRESULT` calls in `GetEndpointId`, `InitializeAudioEndpoint` and
  `ConfigureLiveGraph`, replaced by non-throwing HRESULT tests that take the
  existing degraded paths above.
- The try/catch in `AudioManager::AudioManager`, reduced to the DirectXTK
  exception it still has to catch.

## Out of scope

- `CHECK_HRESULT`, `common::CheckHresult`, and every other Common error macro.
- DirectXTK `AudioEngine` behavior, the silent-mode recovery probe in
  `Update`, device-change `OnReset` handling, and voice subsystems.
- Every other rule 9 site, and every other style guide rule.
- Server code, simulation state, CRC, wire, save, replay and `.pack` data.

## Risk tier and invariants

Change Workflow Tier 2. Trigger: scoped client runtime behavior in one
subsystem (Audio startup failure handling). No determinism/CRC, wire,
serialization, replay, threading or trust-boundary surface is touched: audio
is client-only presentation and the constructor runs on the main thread.

Invariants:

- With every HRESULT succeeding, endpoint choice, engine construction, pin
  attempt, `RegisterNotify` and voice `Init` happen in the same order as today.
- A failed endpoint HRESULT yields the stable silent-capable engine (the
  existing fallback), not a missing one, so `InitializeAudioSubsystems` still
  runs.
- No first-party throw remains inside the constructor's try.

## Acceptance criteria

- `rg -n "CHECK_HRESULT" Engine/Source/Audio/AudioManager.cpp` returns no
  lines in the functions named in `## In scope`.
- Client Debug and Release builds succeed through `/compile`.
- On a client launched through `/agent-harness`, `get_logs` with category
  `Audio` shows the same endpoint-selection lines as a baseline launch on the
  same machine.

## Notes

Found by the rule 9 candidate sweep of
`Documents/Plans/Engine/StyleGuideRepoReviewRuleSweep.md` at baseline
`46241779`; the sites are pre-existing there.
