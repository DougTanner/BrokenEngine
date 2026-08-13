<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-12T18:50:22.956Z","dependsOn":[]} -->
# Add Engine/Source/Frame/FrameUtils.h to both game project files

## Context

`Engine/Source/Frame/FrameUtils.h` is not listed in either game project. A case-insensitive search of
`Projects/BrokenEngineSandbox/Platforms/VisualStudio2026/BrokenEngineSandbox.vcxproj`,
`BrokenEngineSandboxServer.vcxproj`, and both matching `.filters` files returns no hit for `FrameUtils`, while
its sibling `FrameBase.h` — which includes it at `Engine/Source/Frame/FrameBase.h:21` — is listed as a
`ClInclude` in the client project at `:375`, the server project at `:355`, and in both filter files
(`BrokenEngineSandbox.vcxproj.filters:624`, `BrokenEngineSandboxServer.vcxproj.filters:465`).

The header is pure declarations reached only through `#include`, so both builds compile and link exactly as
they do now; nothing is broken at runtime. The cost is IDE-only: the file never appears in Solution Explorer,
so it is invisible to project-tree navigation and to any tooling that enumerates project membership.

Pre-existing, not introduced by any recent session: `FrameUtils.h` has been absent from both projects since
the squashed baseline commit `e571f6f`, and a baseline search returned zero hits. The session that landed
`Documents/Plans/Frame/FrameUtilsSharedHelpers.md` only added content to the existing file — it created no
file and changed no build affinity, so `/update-vcxproj` was correctly not triggered there and the gap was
left untouched.

## Design

Run `/update-vcxproj` for `Engine/Source/Frame/FrameUtils.h` and apply exactly what it prescribes. The header
is build-agnostic — it carries no `BT_CLIENT`/`BT_SERVER` whole-file guard and both executables consume it
through `FrameBase.h` — so it belongs in both the client and the server project, mirroring `FrameBase.h`'s
existing `ClInclude` entries and filter assignment. The skill and
`Projects/BrokenEngineSandbox/Platforms/VisualStudio2026/AGENTS.md` own the exact item group, ordering, and
filter rules; do not hand-author entries against this paragraph.

No source file changes. No compiled translation unit is added or removed, so no build output changes.

## Critical files

- `Projects/BrokenEngineSandbox/Platforms/VisualStudio2026/BrokenEngineSandbox.vcxproj`
- `Projects/BrokenEngineSandbox/Platforms/VisualStudio2026/BrokenEngineSandbox.vcxproj.filters`
- `Projects/BrokenEngineSandbox/Platforms/VisualStudio2026/BrokenEngineSandboxServer.vcxproj`
- `Projects/BrokenEngineSandbox/Platforms/VisualStudio2026/BrokenEngineSandboxServer.vcxproj.filters`

## In scope

- The `ClInclude` entry for `Engine\Source\Frame\FrameUtils.h` in both `.vcxproj` files and its filter
  assignment in both `.filters` files, exactly as `/update-vcxproj` prescribes

## Out of scope

- Any other missing, stale, or misfiled project entry the run happens to surface — record those separately
- Any change to `Engine/Source/Frame/FrameUtils.h` itself or to any other source file
- Build settings, configurations, filter definitions not required by this one entry

## Risk tier and invariants

Tier 1 — mechanical project membership with no public signature or invariant exposure. Invariants: the two
projects stay symmetric for a build-agnostic header; the filter assignment matches `FrameBase.h`'s; no
`ClCompile` item is added, so neither executable gains or loses a translation unit.

## Acceptance criteria

- `/update-vcxproj` reports the membership reconciled with no remaining discrepancy for this file.
- Client and server both still compile.
- A case-insensitive search for `FrameUtils` finds the entry in all four project files.
