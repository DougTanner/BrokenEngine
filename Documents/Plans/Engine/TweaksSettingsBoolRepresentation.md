<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-26T23:40:48.157Z","dependsOn":[]} -->
# Fix: Read the persisted Tweaks ImGui flag as a byte

## Context

The accepted survivor `CAI/shard-0045/002` shows that `TweaksSettings` stores
`bShowImGui` as a raw C++ `bool` and loads the whole POD representation from an
opaque file (`Projects/BrokenEngineSandbox/Source/ClientSettings.cpp:12-23,48-54`).
`common::Read` validates no semantic value; a version/size-valid byte such as
`0x02` is copied into a `bool` object and then read.  The repository already
uses a fixed-width byte for an analogous persisted boolean
(`Engine/Source/Ui/GameSettings.cpp:19-20`).

The source report is
`Temp/CppAdversarialInvariantAudit/76d303f0eeeb86c1ed241edc81634e60070ba5a5/shard-0045.md:54`;
the consolidated selector is
`Temp/CppAdversarialInvariantAudit/76d303f0eeeb86c1ed241edc81634e60070ba5a5/consolidated-index.md:1091`.
All 11 frozen target rows matched baseline
`76d303f0eeeb86c1ed241edc81634e60070ba5a5`; no source changes were made in
this routing session.

Impact: malformed debug settings can invoke undefined behavior while selecting
the ImGui mode instead of being recoverable as an invalid setting.

## Design

Author's recommendation: replace the persisted member with `uint8_t
uiShowImGui`, preserving its one-byte offset, the surrounding padding, and
`TweaksSettings`'s existing size/version.  Save `0` or `1`; on load accept only
those two values, and when any other byte is present log the existing warning
and leave the live Tweaks settings unchanged rather than constructing a
`bool` from an invalid representation.  Keep the rest of the POD fields and
the current versioned-file framing unchanged.

## Critical files

- `Projects/BrokenEngineSandbox/Source/ClientSettings.cpp:12-24,26-59` —
  persisted layout, save, and load.
- `Engine/Source/Ui/GameSettings.cpp:19-20,48-78` — fixed-width persisted
  boolean precedent.
- `Common/Serialization.h:80-92` and `Engine/Source/File/FileManager.h:342-365`
  — raw POD read/version-size boundary.
- `Engine/Source/Main.cpp:287-299` — live debug startup caller.

## In scope

- Replacing only `TweaksSettings::bShowImGui`'s persisted type with a byte and
  validating its value before assigning `gpGame->mbShowImGui`.
- Keeping the byte offset/struct size/version and existing save/load warning
  path stable.
- Updating local comments/static assertions needed to document the fixed-width
  representation.

## Out of scope

- The persisted sun-angle finite check, Tweaks UI state, engine-wide boolean
  serialization policy, or a version migration/new compatibility format.
- Other settings fields, client-state loading, server code, and unit tests.

## Risk tier and invariants

Expected Change Workflow Tier 2 (downgraded from Tier 3). Trigger: opaque
versioned POD input crosses the game startup boundary and changes persisted
representation semantics.

Tier rationale: the Design fixes the exact edit — one member becomes `uint8_t`
at the same offset with the same struct size and version, and load accepts only
0 or 1 through the existing warning path. Valid version-14 files keep identical
bytes and behavior, and the change lives in one game-owned settings file.

Preserve these invariants:

- Only byte values 0 and 1 become the live ImGui boolean.
- Existing valid version-14 files retain the same offsets, size, and behavior.
- An invalid flag cannot create an invalid C++ `bool` object representation or
  partially apply a corrupt settings body.
- No simulation CRC, wire, replay, or `.pack` data changes.

## Acceptance criteria

- A version/size-valid `TweaksSettings.bin` with `uiShowImGui == 0` or `1`
  loads/saves as before.
- A file with byte `0x02` is rejected by the semantic check, leaves current
  live state unchanged, and logs the existing warning without undefined bool
  behavior.
- `sizeof(TweaksSettings)` remains 304 and `kiVersion` remains 14 because the
  bytes/layout are unchanged.
- Client Debug and Release builds pass `/compile`; a debug startup/state-load
  harness run has no invalid-bool or ImGui-mode error.

## Notes

The accepted survivor is local to the game-owned debug settings POD; no generic
serialization redesign is needed.
