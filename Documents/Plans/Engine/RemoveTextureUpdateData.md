<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-14T22:54:42.311Z","dependsOn":[]} -->
# Remove the uncalled Texture::UpdateData and its layout parameters

## Context

Paths are relative to `Engine/Source/Graphics/Objects/`. Verified against
commit `c675b6b2`.

`Texture::UpdateData` (`Texture.h:106`, `Texture.cpp:232-236`) has no caller.
A search of `Engine`, `Projects`, `Tools`, `DataPacker`, and `Common` finds
only its declaration and definition.

It is also the only source of a second value pair for the `eOldLayout` and
`eFinalLayout` parameters of `Texture::UploadImageData` and
`Texture::RecordImageData` (`Texture.h:118-119`): it passes
`kShaderReadOnly, kShaderReadOnly` (`Texture.cpp:235`). The two live callers
pass `kUndefined, mInfo.eTextureLayout` (`Texture.cpp:161` in `Create`,
`:184` in `RecordCreate`), and `UploadImageData` only forwards
(`Texture.cpp:245`). With `UpdateData` gone, both parameters carry one value
pair.

Origin: an architecture review and an in-function refactor review of
`Engine/Source/Graphics/Objects`, independently verified. Unmet criterion:
root `AGENTS.md` `## Directives` minimum sufficient change and YAGNI — no code
without a caller, no parameter with one value.

## Design

Recommended, as one change (removing the parameters alone would break
`UpdateData`):

- Delete `Texture::UpdateData`.
- Remove `eOldLayout` and `eFinalLayout` from `UploadImageData` and
  `RecordImageData`. `RecordImageData` transitions from `kUndefined` and ends
  at `mInfo.eTextureLayout`.
- Keep the "final layout is not `kUndefined`" test before the last
  transition, now on `mInfo.eTextureLayout`: several producers leave
  `eTextureLayout` at `kUndefined`
  (`Engine/Source/Graphics/Managers/SwapchainManager.cpp`,
  `TextureManager.cpp`, `ImGuiManager.cpp`).

## Critical files

- `Engine/Source/Graphics/Objects/Texture.h`
- `Engine/Source/Graphics/Objects/Texture.cpp`

## In scope

- `Texture::UpdateData`, declaration and definition.
- The `eOldLayout` and `eFinalLayout` parameters of `Texture::UploadImageData`
  and `Texture::RecordImageData`, their uses inside `RecordImageData`, and the
  arguments at the call sites in `Texture::Create`, `Texture::RecordCreate`,
  and `UploadImageData`.

## Out of scope

- `Texture::TransitionImageLayout` and its own `eOldLayout` parameter.
- The layout transition `Texture::Create` performs when no data function is
  supplied.
- `TextureLayout` values and `kpLayoutMappings`;
  `Documents/Plans/Engine/RemoveUnproducedGraphicsFlagValues.md` owns the
  unused value.
- `CreateImageView`.

## Risk tier

Tier 2 — scoped behavior. Trigger: a public method of `Texture` is removed,
which Tier 1's "no public signature exposure" excludes. No runtime behavior
changes: every reachable upload records the same transitions. Client-only; no
determinism/CRC, wire, serialization, replay, threading, or shader exposure.

## Acceptance criteria

- A repository search for `UpdateData` finds no `Texture` member.
- In `Texture.h` and `Texture.cpp`, `eOldLayout` appears only as
  `TransitionImageLayout`'s own parameter, and `eFinalLayout` does not appear.
- The client and ModelViewer build.

## Coordination

The Plan listed below edits other regions of `Texture.h` or `Texture.cpp`. It
does not depend on this Plan, and this Plan does not depend on it. Whichever
lands second re-derives its regions by symbol from the landed files:

- `Documents/Plans/Engine/RemoveUnproducedGraphicsFlagValues.md`

## Notes

- The private game project in the primary checkout holds only documents and no
  source, so it has no caller.
- Nothing was built or run by the authoring session; the evidence is from
  reading and searching the tree.
