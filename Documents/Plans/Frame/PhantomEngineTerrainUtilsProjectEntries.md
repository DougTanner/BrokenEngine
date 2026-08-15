<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-15T16:09:59.929Z","dependsOn":[]} -->
# Remove the phantom Engine/Source/Frame/TerrainUtils.h project entries

## Context

Both game project files list a header that does not exist anywhere in the
repository:

- `Projects/BrokenEngineSandbox/Platforms/VisualStudio2026/BrokenEngineSandbox.vcxproj:384` —
  `<ClInclude Include="..\..\..\..\Engine\Source\Frame\TerrainUtils.h" />`
- `Projects/BrokenEngineSandbox/Platforms/VisualStudio2026/BrokenEngineSandbox.vcxproj.filters:816` —
  the same include with `<Filter>Engine\Frame</Filter>`
- `Projects/BrokenEngineSandbox/Platforms/VisualStudio2026/BrokenEngineSandboxServer.vcxproj:364` —
  the same include
- `Projects/BrokenEngineSandbox/Platforms/VisualStudio2026/BrokenEngineSandboxServer.vcxproj.filters:552` —
  the same include with `<Filter>Engine\Frame</Filter>`

`Engine/Source/Frame/TerrainUtils.h` is absent from disk, absent from
`git ls-files`, and no commit reachable from any ref ever added it
(`git log --all --oneline -- Engine/Source/Frame/TerrainUtils.h` is empty). The
real header is the game-side
`Projects/BrokenEngineSandbox/Source/Frame/TerrainUtils.h`, listed separately in
both projects (`BrokenEngineSandbox.vcxproj:470`,
`BrokenEngineSandboxServer.vcxproj:410`) and unaffected here.

The four entries are present unchanged at commit `f1090e4`, the baseline of the
`Documents/Plans/Frame/TerrainTraceToEngine.md` session that found them, so they
are pre-existing. That Plan's `## In scope` covered only moving `SegmentHit`,
`TracePointAgainstTerrain`, and `TracePointToFrameExit` and requalifying their
call sites, and it explicitly recorded that `/update-vcxproj` was not triggered;
project membership repair was outside its boundary.

A phantom `ClInclude` makes the IDE show a missing file under `Engine\Frame` and
makes membership audits reason about a path that has no content.

## Design

Delete the four `ClInclude` entries named above — the two bare entries in the
`.vcxproj` files and the two `<ClInclude>...<Filter>Engine\Frame</Filter></ClInclude>`
blocks in the `.filters` files. Nothing else moves: the game-side
`..\..\Source\Frame\TerrainUtils.h` entries and their filters stay exactly as
they are, and no source file changes.

## Critical files

- `Projects/BrokenEngineSandbox/Platforms/VisualStudio2026/BrokenEngineSandbox.vcxproj` (`:384`)
- `Projects/BrokenEngineSandbox/Platforms/VisualStudio2026/BrokenEngineSandbox.vcxproj.filters` (`:816`)
- `Projects/BrokenEngineSandbox/Platforms/VisualStudio2026/BrokenEngineSandboxServer.vcxproj` (`:364`)
- `Projects/BrokenEngineSandbox/Platforms/VisualStudio2026/BrokenEngineSandboxServer.vcxproj.filters` (`:552`)

## In scope

- Removing exactly the four `Engine\Source\Frame\TerrainUtils.h` `ClInclude`
  entries listed under `## Critical files`
- Running `/update-vcxproj` to confirm the resulting membership validates

## Out of scope

- The game-side `Projects/BrokenEngineSandbox/Source/Frame/TerrainUtils.h`
  entries and their `Game\Frame` filters
- Any other membership, filter, or project-setting difference between the two
  projects
- Any C++ source, header, or include change
- Creating an `Engine/Source/Frame/TerrainUtils.h`

## Risk tier and invariants

Expected Tier 1 (project membership, behavior preserving). No file changes which
executable it belongs to and no compiled translation unit is added or removed,
so client and server binaries are unaffected.

## Acceptance criteria

- No `Engine\Source\Frame\TerrainUtils.h` string remains in any `.vcxproj` or
  `.filters` file
- `/update-vcxproj` reports membership valid for both projects
- Client and server both compile

## Notes

Found while landing `Documents/Plans/Frame/TerrainTraceToEngine.md`; recorded as
a pre-existing residual rather than fixed inside that change.
