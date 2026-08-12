<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-12T02:20:31.665Z","dependsOn":[]} -->
# Remove the dead Font.h project entries and the duplicate NotoSansSC asset

## Context

The old font system was removed; fonts now ship through the `Raw` data type.
`DataPacker/Source/Main.cpp:29-39` lists every generated data type — `Audio`,
`Scene`, `Islands`, `Model`, `Shader`, `Texture`, `Raw` — with no `Font` type, so
`$(GameDataDirectory)\Font.h` is never generated. Two leftovers remain tracked:

- Four project entries reference that never-generated header:
  `Projects/BrokenEngineSandbox/Platforms/VisualStudio2026/BrokenEngineSandbox.vcxproj:507`,
  `BrokenEngineSandbox.vcxproj.filters:681-683` (filter `DataFiles`),
  `BrokenEngineSandboxServer.vcxproj:438`, and
  `BrokenEngineSandboxServer.vcxproj.filters:504-506`. Repository-wide, those
  four lines are the only remaining references to `Font.h`.
- `Engine/Data/Fonts/NotoSansSC/` holds a tracked duplicate of the shipping font.
  `Engine/Data/Fonts/NotoSansSC/NotoSansSC-Light.otf` and
  `Engine/Data/Raw/NotoSansSC-Light.otf` are byte-identical (both Git blob
  `85ccdf44a4c5e47be4b581e8bb1ddff4f4773a31`); the `Raw` copy is the live one, and
  it carries its own full SIL Open Font License text in the sidecar
  `Engine/Data/Raw/NotoSansSC-Light.txt`. Nothing in `DataPacker`,
  `Engine/Source`, `Projects/*/Source`, or `.agents` reads
  `Engine/Data/Fonts` — the directory has no consumer at all. Its two remaining
  files, `OFL.txt` and `README.txt`, are that duplicate's own licence and vendor
  readme.

Both leftovers predate the session that recorded this Plan and lie outside its
approved boundary.

## Design

Delete the four `Font.h` project entries through `/update-vcxproj`, which owns
membership and filter reconciliation, and delete the orphaned
`Engine/Data/Fonts/NotoSansSC/` directory whole — the duplicate asset together
with the `OFL.txt` and `README.txt` that exist only to accompany it. Removing a
licence file only alongside the asset it covers is what keeps
`Engine/Data/AGENTS.md`'s "preserve per-asset licenses and attribution sidecars"
rule satisfied: after the deletion the only shipping copy of the font is
`Engine/Data/Raw/NotoSansSC-Light.otf`, whose sidecar still carries the complete
licence.

## Critical files

- `Projects/BrokenEngineSandbox/Platforms/VisualStudio2026/BrokenEngineSandbox.vcxproj`
- `Projects/BrokenEngineSandbox/Platforms/VisualStudio2026/BrokenEngineSandbox.vcxproj.filters`
- `Projects/BrokenEngineSandbox/Platforms/VisualStudio2026/BrokenEngineSandboxServer.vcxproj`
- `Projects/BrokenEngineSandbox/Platforms/VisualStudio2026/BrokenEngineSandboxServer.vcxproj.filters`
- `Engine/Data/Fonts/NotoSansSC/NotoSansSC-Light.otf`, `OFL.txt`, `README.txt`

## In scope

- The four `$(GameDataDirectory)\Font.h` `ClInclude` entries and their filter
  blocks, removed through `/update-vcxproj`
- Deletion of the tracked `Engine/Data/Fonts/NotoSansSC/` directory and its three
  files

## Out of scope

- `Engine/Data/Raw/NotoSansSC-Light.otf` and `Engine/Data/Raw/NotoSansSC-Light.txt`
- Every other project entry, filter, and build setting
- `DataPacker`'s data-type table and any font loading or rendering code
- Machine-local cache and output directories, which are untracked

## Risk tier and invariants

Expected Change Workflow Tier 1 — project membership plus deletion of a file with
no consumer, with no public signature, invariant, or runtime behavior exposure.
Invariant: the shipping font and its licence text must remain tracked and
unchanged.

## Acceptance criteria

- `Font.h` appears nowhere in the repository outside generated output directories
- `Engine/Data/Fonts` is gone from the Git index, and
  `Engine/Data/Raw/NotoSansSC-Light.otf` with its `.txt` sidecar is untouched
- `/update-vcxproj` reports both project/filter pairs consistent
- Client and server projects build
