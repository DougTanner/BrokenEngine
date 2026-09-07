<!-- broken-engine-plan/v1 {"createdUtc":"2026-09-07T14:39:03.150Z","dependsOn":[]} -->
# Trim the File leaf AGENTS.md in place

## Context

`Engine/Source/File/AGENTS.md` is automatically loaded as the File subsystem's
instruction document. It currently measures 3,605 `bt-token-v1` (64 lines,
14,419 bytes), above the 2,000-token leaf target in
`.agents/skills/update-claude-docs/references/audit-mode.md`. The measurement is
from:

`pwsh -NoProfile -File .agents/scripts/Measure-Tokens.ps1 Engine/Source/File/AGENTS.md -Json`

The file's contracts remain necessary, but many are expressed with repeated
subjects, explanatory consequences, and multi-sentence narration. The file can
stay in its current automatically loaded location while stating the same rules
more directly.

## Design

Trim `Engine/Source/File/AGENTS.md` in place. Keep its title, intro, five
contract sections, and `## See Also` section so existing ownership and anchors
remain stable. Do not create detail documents or redirect readers elsewhere.

Condense each section by applying the same rule: retain every unique ownership,
ordering, validation, failure, lifetime, concurrency, versioning, and
determinism requirement, while removing repeated setup, duplicate rationale,
and examples whose operative rule is already stated. Combine adjacent bullets
when they govern the same operation, and replace narrative cause-and-effect
wording with the shortest direct instruction that preserves both the condition
and required outcome.

The trim must preserve these section responsibilities:

- `## File Contracts`: data and AppData path selection, versioned payloads,
  failure reporting, and atomic versus streaming writes.
- `## Grid Saves`: deterministic writes, staged validation and adoption,
  trust-boundary checks and failure split, island-template handling, the game
  payload boundary, one-way opacity, and direct-include requirement.
- `## Packed Assets` and `## Lazy-Pool Invariants`: eager/lazy ownership,
  loader synchronization and reset preconditions, integrity-token ordering,
  manifest/chunk validation and fatal corruption policy, stable pool layout,
  reclamation, and recommit requirements.
- `## Replay Streams`: ownership and debug gating, manifest commit and
  validation, generation ordering and retirement, staged adoption and abort
  behavior, transfer handling, playback-state publication, loop timing,
  checksums, game-owned metadata, diagnostics, and post-dispatch channel rules.

Preserve the meaning of every contract, including exceptions and negative
requirements. Concise rewording is allowed; weakening, broadening, or deleting
a contract is not.

Change Workflow tier: Tier 1, mechanical documentation-only work. It changes no
public signature, runtime behavior, determinism/CRC calculation, serialization
or replay format, wire contract, threading behavior, C++, GLSL, script, or
project membership.

## Critical files

- `Engine/Source/File/AGENTS.md`

## In scope

- Condense the existing title, intro, `## File Contracts`, `## Grid Saves`,
  `## Packed Assets`, `## Lazy-Pool Invariants`, `## Replay Streams`, and
  `## See Also` prose in `Engine/Source/File/AGENTS.md` until the whole file is
  below 2,000 `bt-token-v1`.
- Preserve every unique contract and exception represented by the current
  document, using concise wording and merged bullets where meaning remains the
  same.
- Keep the document at the same path with the same section headings and link
  targets so it remains automatically loaded and existing anchors stay valid.

## Out of scope

- Splitting the document, creating linked detail documents, moving guidance to
  another file, or changing inbound references.
- Adding, removing, weakening, broadening, or correcting a File subsystem
  contract.
- Editing `Engine/Source/File/CLAUDE.md`, another `AGENTS.md`, a skill or its
  references, or any other instruction-document remediation.
- Any C++, GLSL, script, project-file, runtime, data-format, or build change.

## Invariants

- Ordinary instruction loading continues through the existing
  `Engine/Source/File/AGENTS.md` and `Engine/Source/File/CLAUDE.md` pair.
- Every pre-trim operative rule, exception, and required outcome remains
  semantically present in the trimmed file.
- Existing headings, relative links, and inbound anchors continue to resolve.
- The change reduces wording only; it does not alter documented engine policy.

## Acceptance criteria

- `pwsh -NoProfile -File .agents/scripts/Measure-Tokens.ps1 Engine/Source/File/AGENTS.md -Json`
  reports fewer than 2,000 `bt-token-v1`.
- A before/after contract inventory maps every current operative rule,
  exception, and required outcome to semantically equivalent trimmed text, with
  no unmatched item.
- The diff changes only `Engine/Source/File/AGENTS.md`, preserves its existing
  section headings and link destinations, and adds no new document.
- The applicable Markdown static check reports that every changed-file link and
  heading anchor resolves.
