<!-- broken-engine-plan/v1 {"createdUtc":"2026-09-07T14:39:03.150Z","dependsOn":[]} -->
# Split the File leaf AGENTS.md into detail documents

## Context

`Engine/Source/File/AGENTS.md` measures 3,587 `bt-token-v1` (64 lines,
14,346 bytes) against the 2,000-token leaf target that
`.agents/skills/update-claude-docs/references/audit-mode.md` and
`.agents/skills/progressive-disclosure-review/references/worker.md` apply, a
79% overrun. Measured with the documented route,
`pwsh -NoProfile -File .agents/scripts/Measure-Tokens.ps1 Engine/Source/File/AGENTS.md -Json`,
on baseline 96477b68162d0ab748d798b4e16199d7228d69e0.

Observed by the `/update-claude-docs` pass of a whitespace-only C++ formatting
session, whose own change touched no documentation; the overrun is pre-existing
and outside that session's implementation boundary, and that skill forbids
trimming pre-existing unrelated excess.

Section sizes, measured the same way:

| Section | Lines | `bt-token-v1` |
| --- | --- | --- |
| Intro + `## File Contracts` | 1-11 | 395 |
| `## Grid Saves` | 13-24 | 605 |
| `## Packed Assets` + `## Lazy-Pool Invariants` | 26-41 | 962 |
| `## Replay Streams` | 43-60 | 1,600 |
| `## See Also` | 62-65 | ~15 |

The three headline sections describe three separate implementations the intro
already names apart: `GridSave`, `PackChunks`/`PackChunkLoader`, and `Replay`.

## Design

Author's recommendation: apply the repository's existing detail-document
pattern, sibling `<Name>.AGENTS.md` files linked from a leaf that keeps only
the shared contracts and a responsibility map, exactly as
`Engine/Source/Graphics/Managers/AGENTS.md` links `TextureManager.AGENTS.md`
and its siblings. That pattern is already recognized by tooling: the
`/update-claude-docs` stub-pair sweep excludes linked `*.AGENTS.md` reference
files, so the new documents need no `CLAUDE.md` stubs.

Recommended split, moving prose verbatim rather than rewriting it:

- `Engine/Source/File/Replay.AGENTS.md` — the whole `## Replay Streams` body
  (~1,600 tokens), covering `Replay.h/.cpp` and `DifferenceStream.h`.
- `Engine/Source/File/PackChunks.AGENTS.md` — `## Packed Assets` plus
  `## Lazy-Pool Invariants` (~962 tokens), covering `PackChunks.h/.cpp` and
  `PackChunkLoader.h/.cpp`.
- The leaf keeps the intro, `## File Contracts`, `## Grid Saves`, and
  `## See Also`, and gains a short responsibility map linking the two new
  documents. Projected leaf size ~1,100 `bt-token-v1`, comfortably inside the
  2,000 target; each new document is itself under it.

`## Grid Saves` stays in the leaf because moving it is not needed to reach the
target and the leaf would otherwise carry no subsystem contract of its own.

Relative-link depth changes for links that move: paths inside the moved prose
already resolve from the same directory, so they carry over unchanged; verify
each one after the move rather than assuming.

Inbound anchor links must be retargeted, because the anchor leaves the leaf:
`Projects/BrokenEngineSandbox/Source/Network/Server/AGENTS.md` lines 24 and 25
both link `Engine/Source/File/AGENTS.md#replay-streams`. Every other inbound
reference points at the leaf without an anchor and stays valid, but each one
whose specific fact moved should point at the new document instead; the
candidates are in `Engine/Source/AGENTS.md`, `Engine/Source/Audio/AGENTS.md`,
`Engine/Source/Graphics/AGENTS.md`,
`Engine/Source/Graphics/Managers/AGENTS.md`,
`Engine/Source/Graphics/Managers/TextureManager.AGENTS.md`,
`Engine/Source/Graphics/Managers/TextureUploadManager.AGENTS.md`,
`Engine/Source/Network/Server/AGENTS.md`, `Common/AGENTS.md`,
`DataPacker/Source/ExportJobs/Texture/AGENTS.md`,
`Projects/BrokenEngineSandbox/Source/AGENTS.md`,
`Projects/BrokenEngineSandbox/Source/Frame/AGENTS.md`,
`Projects/BrokenEngineSandbox/Source/Network/Server/AGENTS.md`,
`Projects/BrokenEngineSandbox/Source/Save/AGENTS.md`,
`.agents/skills/agent-harness/references/worker.md`, and
`.agents/skills/repo-code-review/references/checks.md`.

Change Workflow tier: Tier 1, mechanical — documentation only, with no public
signature or invariant exposure. No C++, shader, build, determinism/CRC,
serialization, replay-format, wire, or threading surface is touched; the
documented invariants themselves must survive the move word for word.

## Critical files

- `Engine/Source/File/AGENTS.md`
- `Engine/Source/File/Replay.AGENTS.md` (new)
- `Engine/Source/File/PackChunks.AGENTS.md` (new)
- `Projects/BrokenEngineSandbox/Source/Network/Server/AGENTS.md`
- `Engine/Source/Graphics/Managers/AGENTS.md` (pattern exemplar, read only)

## In scope

- Create `Engine/Source/File/Replay.AGENTS.md` holding the moved
  `## Replay Streams` content, and `Engine/Source/File/PackChunks.AGENTS.md`
  holding the moved `## Packed Assets` and `## Lazy-Pool Invariants` content.
- Remove those three sections from `Engine/Source/File/AGENTS.md` and add a
  responsibility map linking the two new documents.
- Retarget the two `#replay-streams` anchor links in
  `Projects/BrokenEngineSandbox/Source/Network/Server/AGENTS.md` to the
  corresponding anchor in the new Replay document.
- Repoint any inbound reference listed in `## Design` whose cited fact now
  lives in a new document.

## Out of scope

- Rewriting, condensing, correcting, or adding any documented contract: the
  moved prose changes only in location and in link targets.
- `Engine/Source/File/CLAUDE.md`, which keeps its `@AGENTS.md` stub bytes;
  no new `CLAUDE.md` stubs.
- Moving `## Grid Saves`, `## File Contracts`, or the intro out of the leaf.
- Any C++, GLSL, script, or project-file change.
- Trimming any other oversized AGENTS.md.

## Acceptance criteria

- `pwsh -NoProfile -File .agents/scripts/Measure-Tokens.ps1 Engine/Source/File/AGENTS.md -Json`
  reports under 2,000 `bt-token-v1`, and each new document does too.
- Every sentence removed from the leaf appears verbatim in exactly one new
  document; no contract sentence is lost or reworded.
- No repository link resolves to a missing file or a missing anchor.

## Notes

Recorded as a follow-up under the root `AGENTS.md` Leftovers rule; deferral is
main-session routing, not a user direction.
