<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-14T22:54:29.277Z","dependsOn":[]} -->
# AnimationData::Load Rejects Every Corrupt Field Through One Path

## Context

Found by a deep analysis of `Engine/Source/Graphics` (direct files only) and
verified independently. Pre-existing at commit `401e4b75`; line numbers are
from that commit, so locate by symbol if they moved. `G/` abbreviates
`Engine/Source/Graphics/`.

`AnimationData::Load` validates pack bytes once at load so per-frame
evaluation stays unchecked (its own comment at `G/AnimationData.cpp:110-111`).
Its corrupt-chunk checks throw `std::ios_base::failure("AnimationData::Load")`,
which the boot caller catches to log the chunk's CRC and rethrow
(`:490-495`). Two pack-derived conditions do not follow that path.

1. One check uses `ASSERT`. The condition commented "Corrupt chunk: a skinned
   material in a skeleton with no skin joints" (`:126`) is the only `ASSERT`
   among the corrupt-chunk checks. `common::Assert` throws
   `std::runtime_error` in every build (`Common/ErrorUtils.h:20`,
   `Common/ErrorUtils.cpp:14-21`), which the caller's
   `catch (const std::ios_base::failure&)` does not take, so this one corrupt
   shape skips the "Corrupt animation data for GLTF CRC" log line. Both paths
   already halt and both log at `kError`; the `ASSERT` log carries the
   expression and file and line, and the `ASSERT` also triggers `DEBUG_BREAK`.
2. One field is not checked. `AnimationChannel::uiTargetPath` is a `uint8_t`
   read from the pack (`Common/DataFile.h:164`, valid values
   `kiTargetPathTranslation` to `kiTargetPathScale` at `:156-158`). The
   channel check at `AnimationData.cpp:132-133` validates the node index and
   keyframe range but not this field, and `AnimationData.cpp` is its only
   engine reader. A value above `kiTargetPathScale` passes `Load` and reaches
   `default: ASSERT(false); std::unreachable();` (`:386-388`) on each
   evaluated frame, possibly on a worker thread. `ASSERT(false)` always
   throws, so there is no undefined behavior today; the failure is late and
   does not name the chunk. (`uiInterpolation` is also unchecked, but an
   unknown value falls through to the linear path and has no failing arm.)

Authority contradiction, surfaced and not resolved here. Two documents name
the mechanism for a corrupt runtime pack chunk and disagree:

- `Engine/Source/File/AGENTS.md` `## Packed Assets`: "Corrupt runtime `.pack`
  chunks and pack-backed textures halt both processes, even at call sites
  outside this subsystem: validate and `ASSERT`; never soft-fail, skip, or
  substitute a placeholder."
- `Engine/Source/Graphics/Managers/AGENTS.md` `## Shared Contracts`: "Boot
  consumers throw `std::ios_base::failure`; the texture upload thread and
  per-frame adoption `ASSERT`".

`Load` is a boot consumer (`AnimationData.cpp:478-480`). This Plan trusts the
Managers sentence and the code comment at `:478-480`, which the rest of
`Load` already follows; the File sentence read alone endorses the existing
`ASSERT`. Caveat on the trusted side: the Managers sentence lives in the
`Graphics/Managers` leaf `AGENTS.md`, while `AnimationData.cpp` sits in the
parent `Engine/Source/Graphics` directory, so that leaf does not cover the
file by location. Both agree on the outcome (a halt, never a soft failure);
only the mechanism differs. The plan review at claim time confirms which side
governs item 1; if the File sentence governs, item 1 is dropped and item 2
stands.

Unmet criteria: every corrupt-chunk check in `Load` leaves through the path
the caller's catch reports; every channel field that evaluation switches on
is checked in `Load`.

## Design

Recommended:

1. Replace the `ASSERT` at `:126` with the function's own reject: test the
   negated condition and throw `std::ios_base::failure("AnimationData::Load")`.
   The gain is the chunk CRC in the log and one mechanism inside `Load`; the
   cost is losing `DEBUG_BREAK` on this case.
2. Add `rChannel.uiTargetPath > common::AnimationChannel::kiTargetPathScale`
   to the existing channel rejection at `:132-133`. The `default` arm at
   `:386-388` then matches its `std::unreachable()` and stays.

The condition in item 1 is false for exporter output, and a valid pack
carries only the three target paths, so valid packs load unchanged.

## Critical files

- `Engine/Source/Graphics/AnimationData.cpp`

## In scope

- The `ASSERT` at `AnimationData.cpp:126` in `AnimationData::Load`.
- The channel rejection condition at `AnimationData.cpp:132-133` in
  `AnimationData::Load`.

## Out of scope

- `Engine/Source/File/AGENTS.md` and
  `Engine/Source/Graphics/Managers/AGENTS.md`: reconciling the two sentences
  is not part of this Plan; the executing session reports the contradiction
  as a residual naming both sides and which was trusted.
- The `default` arm in `EvaluateWorldMatrices` and `uiInterpolation`.
- The pack format, DataPacker, and `kiVersion`: no byte changes.
- The catch in `LoadAnimationDataFromEagerChunks`.

## Risk tier

Tier 2 — scoped behavior. Trigger: it tightens checks inside one unit at an
existing boundary (pack bytes parsed at boot), with the format and the trust
unchanged. No determinism/CRC, serialization or `.pack` layout, wire, replay,
or threading exposure.

## Acceptance criteria

- No `ASSERT` remains among the corrupt-chunk checks of `Load`, unless the
  plan review dropped item 1.
- The channel rejection in `Load` tests `uiTargetPath`.
- The BrokenEngineSandbox client and ModelViewer build, and the client boots
  with the current packs with no new log line.

## Coordination

No directional dependencies or mandatory nondirectional coordination.

## Notes

- `Documents/Plans/Engine/SkinnedMaterialCountReadsMember.md` and
  `Documents/Plans/Engine/ReuseComputedInverseAndEyeNormal.md` edit other
  functions of `AnimationData.cpp`.
