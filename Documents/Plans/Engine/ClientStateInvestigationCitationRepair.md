<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-20T17:43:10.917Z","dependsOn":[]} -->
# Repoint the stale Input.cpp citation in the ClientState save-lifecycle investigation

## Context

`Documents/Investigations/Engine/ClientStateSaveLifecycle.md:74` cites
`Projects/BrokenEngineSandbox/Source/Input/Input.cpp:150` as the site of the
scroll-delta arbitration that gates `mfCameraEyeHeightTarget` writes. That file no
longer exists: the landed Plan `Documents/Plans/Input/DisplayInputToEngine.md`
moved display input from the game to the engine, and
`Projects/BrokenEngineSandbox/Source/Input/` is gone from the tree.

The equivalent code now lives in `Engine/Source/Input/Input.cpp`: the
user-interface wheel-ownership test at lines 101-114 and the resulting
`gpCamera->mCameraInput.iScrollDelta` assignment at line 118, which is zero
whenever the UI owns the wheel.

This was proven out of scope for the landing change:
`Documents/Investigations/AGENTS.md` makes `Documents/Investigations` a
non-executable findings tree that WorktreeCli never validates or claims, and the
landed Plan's scope covered the input move, not investigation records.

## Design

In `Documents/Investigations/Engine/ClientStateSaveLifecycle.md`, replace the
`Projects/BrokenEngineSandbox/Source/Input/Input.cpp:150` citation at line 74 with
`Engine/Source/Input/Input.cpp:118`, verifying at implementation time that the
line still holds the `iScrollDelta` assignment and adjusting the number if the
file has moved on. Leave the surrounding sentence, the companion
`Projects/BrokenEngineSandbox/Source/Graphics/Camera.cpp:266-283` citation, and
every other part of the document unchanged unless that companion citation is also
found stale, in which case repoint it the same way.

## Critical files

- `Documents/Investigations/Engine/ClientStateSaveLifecycle.md`

## In scope

- The stale path/line citation at line 74 of that document, and the companion
  `Camera.cpp` citation on the following line only if it is likewise stale.

## Out of scope

- Every other file in `Documents/Investigations/`, and every other section,
  question, or conclusion of this document.
- Any code change, and any answer to the open questions the document poses.

## Risk tier and invariants

Expected Tier 1 (documentation-only citation repair in a non-executable
investigation record; no code, invariant, or tool behavior exposure).

## Acceptance criteria

- `Documents/Investigations/Engine/ClientStateSaveLifecycle.md` contains no
  `Projects/BrokenEngineSandbox/Source/Input/` path.
- The replacement citation resolves to an existing file and to the line holding
  the wheel-arbitration scroll-delta assignment.
- `plan validate` exits `0`.
