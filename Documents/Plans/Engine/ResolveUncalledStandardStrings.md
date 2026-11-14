<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-14T22:59:29.618Z","dependsOn":[]} -->
# Delete Nine StandardString Values With No Caller

## Context

`engine::StandardString` (`Engine/Source/Ui/LocalizationBase.h:39-62`) has
nineteen values. Nine have no caller anywhere in Engine, Common, DataPacker,
Tools, BrokenEngineSandbox, or ModelViewer: `kStringComplete`,
`kStringGameOver`, `kStringGamepad`, `kStringMouse`, `kStringMoveWith`,
`kStringOr`, `kStringStart`, `kStringReady`, and `kStringRespawn`
(`LocalizationBase.h:41-55`). A whole-word search for each finds only its
enumerator and the row comment of its translation row in
`LocalizationBase.cpp` (`:9`, `:27`, `:36`, `:63`, `:72`, `:90`, `:117`,
`:126`, `:135`). The other ten values each have callers. Nothing indexes the
enum generically except the uppercase loop in `InitializeLocalization`.

The game facade re-exports the enum, so a game reaches these names through its
own header. The nine are gameplay and input-prompt text, the kind a game HUD
shows. The private game project has no code yet (the user's statement,
2026-11-14), so the searched tree is the complete set of callers and the nine
are dead.

`Engine/Source/Ui/AGENTS.md` `## Localization` describes `StandardString` as
holding "the standard menu strings", which the nine are not, and
`Engine/Source/AGENTS.md` `## Hub Conventions` wants an engine-owned enum value
that names a game concept recorded as deliberate ownership in a leaf document.
The ten values with callers are all menu strings, so deleting the nine makes
that description correct as written.

Originating gap: a deep analysis of `Engine/Source/Ui` — an
architecture-review finding, independently verified for the searchable tree.
Unmet criterion: every `StandardString` value either has a caller or is
recorded as deliberately engine-owned for games.

## Design

User decision, recorded by the authoring agent from the user's own words
(2026-11-14): asked what to do with the nine values, the user answered "Delete
them".

Delete the nine enumerators from `StandardString` and their nine rows, with
the row comments, from `sppTranslatedStrings` in `LocalizationBase.cpp`. The
`static_assert` at `LocalizationBase.cpp:194` catches a mismatch between the
two. The enum is not persisted and its values are never sent, so renumbering
the rest is safe.

## Critical files

- `Engine/Source/Ui/LocalizationBase.h`
- `Engine/Source/Ui/LocalizationBase.cpp`

## In scope

- The nine enumerators named in `## Context` and their nine rows in
  `sppTranslatedStrings`.

## Out of scope

- The ten values with callers, the `Language` enum, `TranslatedString`, and
  `InitializeLocalization`.
- The translations themselves.
- The game facade headers.
- `Engine/Source/Ui/AGENTS.md`, whose `## Localization` wording the deletion
  makes correct.

## Acceptance criteria

- The diff removes exactly the nine enumerators and their nine rows; the
  BrokenEngineSandbox client and server and ModelViewer build; and through
  `/agent-harness` the main menu and pause menu still show their labels in the
  selected language (the supplied evidence records no harness setup;
  `describe_ui` on those two screens serves).

## Notes

- Change Workflow tier: Tier 2 (the author's classification). Trigger:
  removing values from an enum declared in an engine header that game code
  re-exports, a public-interface change Tier 1 excludes. No determinism/CRC,
  wire, serialization, replay, threading, allocation, or shader exposure.
