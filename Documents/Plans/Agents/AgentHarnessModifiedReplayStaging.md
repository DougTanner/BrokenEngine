<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-14T17:29:46.923Z","dependsOn":[]} -->
# Fix: agent-harness / AgentHarness.md — no documented recipe for staging a byte-modified replay artifact with a repaired manifest

## Context

Harness verification of the version-mismatch and corrupt-replay acceptance
criteria required altering a staged replay artifact under the harness AppData
root (`Temp/AppData/Broken Engine Sandbox Server/F7.replay.0`) and then making the
v3 manifest agree with the altered bytes again.

`Projects/BrokenEngineSandbox/Documents/AgentHarness.md:115-126`
(`#### Replay manifest v3 integrity matrix`) documents the manifest layout and the
generation-digest preimage in prose — fixed-width little-endian records, the
ordered `(kind,coordKey,byteCount,sha256[32])` inventory, the kind numbering, and
the exact `broken-engine/replay-manifest-generation/v3` preimage — and it
documents rejection cases that deliberately corrupt the manifest. It documents no
procedure for the opposite operation: changing an artifact's bytes and repairing
the manifest so the replay still loads. `:122` states only "Change one byte or
byte count of each inventory kind", which is a rejection case, not a repair.

Nothing in the harness skill package supplies it either: `agent-harness/SKILL.md`
makes `Projects/<Project>/Documents/AgentHarness.md` the owner of every
verification recipe (`:18`, `:24`), and no file under
`.agents/skills/agent-harness/` mentions the manifest at all — no script under
`.agents/skills/agent-harness/scripts/` performs this staging.

The forced rework: the worker had to reconstruct the procedure from the layout
prose — recompute the altered artifact's inventory entry hash and byte count,
rewrite that entry in place preserving inventory ordering, then recompute the
trailing 32-byte generation digest over the semantic payload — and then
self-validate that reconstruction before it could be used, because a mistake in
it is indistinguishable from the corrupt-data rejection the test was trying to
tell apart from a version mismatch.

The claimed Plan for this run was
`Documents/Plans/Engine/FailureReportingOutReferenceConvention.md`, whose
`## In scope` covers only `engine::DifferenceStreamReader` and its game consumer
in C++ plus one `Engine/Source/File/AGENTS.md` sentence. The harness skill and
the harness document named below are outside that boundary, so this is
`/next-plan` tooling friction rather than an in-scope acceptance failure.

Session provenance (machine-local; not reproducible after cleanup):
- Client: claude
- Session: 68da05af-c6e0-4e32-979d-87ba545ba868
- Session branch: claude/68da05af-c6e0-4e32-979d-87ba545ba868
- Worktree: .claude\worktrees\BrokenEngine\68da05af-c6e0-4e32-979d-87ba545ba868
- Landing commit: `git log --diff-filter=A --format=%H -- <this plan path>`
- Run the review before /cleanup-worktrees removes this worktree: Codex
  transcript discovery requires the producing worktree to remain registered,
  and Claude review requires the exact session id above.

## Design

In a new session, run `/next-plan-review <landing commit>` supplying the
recorded client and session id, root-cause the friction from the proven
transcript, then make the smallest fix inside the `## In scope` boundary below.
If root-causing shows the fix lies outside that boundary, surface it for
re-planning instead of expanding scope.

The outcome to deliver: an agent that must stage a byte-modified replay artifact
and keep the manifest valid can do so from the documented procedure alone,
without reconstructing and self-validating the hash-and-digest repair from the
layout prose. Two candidate shapes are visible from the symptom and root-causing
decides between them — they are alternatives, not a set to implement together: a
short recipe stated beside the existing integrity matrix, or a bundled helper
script under `.agents/skills/agent-harness/scripts/` that performs the repair and
that the document cites.

## Critical files

- `Projects/BrokenEngineSandbox/Documents/AgentHarness.md` — the
  `#### Replay manifest v3 integrity matrix` section (`:115-126`), which owns the
  manifest layout, the generation-digest preimage, and the backup/restore
  discipline the recipe builds on
- `.agents/skills/agent-harness/SKILL.md` — the sentences making
  `Projects/<Project>/Documents/AgentHarness.md` the owner of verification
  recipes (`:18`, `:24`), read-only unless the chosen shape adds a bundled script
  that must be named there
- `.agents/skills/agent-harness/scripts/` — the bundled-script location, relevant
  only if root-causing selects the helper-script shape

## In scope

- Root-cause investigation via `/next-plan-review` with the recorded provenance
- The smallest resulting fix, confined to the
  `#### Replay manifest v3 integrity matrix` section of
  `Projects/BrokenEngineSandbox/Documents/AgentHarness.md` plus, only if
  root-causing selects the helper-script shape, one new script under
  `.agents/skills/agent-harness/scripts/` and the `agent-harness/SKILL.md`
  sentence that names it

## Out of scope

- The landed change this session produced, and its claimed Plan
- Any change to the replay manifest format, its version, the generation-digest
  preimage, the inventory layout, or any engine or game code that writes or
  validates them
- The existing H1-H6 rejection cases and their required outcomes, which stay
  exactly as they are
- Any harness command, response field, or schema change
- Unrelated skills/scripts; any transcript path or transcript text in the repo

## Risk tier and invariants

Expected Tier 2 (scoped harness tool behavior and its verification
documentation); Tier 1 if the accepted fix is documentation only. Escalate if the
fix reaches replay format, serialization, or engine validation code — that is
outside the boundary above and is surfaced for re-planning instead. The recipe
must keep the existing backup-and-restore discipline stated at `:117` so a staged
modification never becomes permanent, and must not weaken any rejection case:
making a valid modified artifact easier to stage must not make an invalid one
loadable. Never embed transcript paths or home paths.

## Acceptance criteria

- The recorded symptom no longer reproduces: an agent following the documented
  procedure stages a byte-modified replay artifact whose manifest the reader
  accepts, without reconstructing the inventory-hash or generation-digest
  computation itself
- The procedure states the exact steps for the artifact's inventory entry (byte
  count and SHA-256) and the trailing generation digest, and preserves inventory
  ordering
- The existing H1-H6 rejection cases still reject, unchanged
- `/validate-skill` passes for any changed `SKILL.md`; WorktreeCli
  `plan validate` exits `0` with `status: valid` and `code: ok`

## Notes

This Plan is keyed to the pair (`Projects/BrokenEngineSandbox/Documents/AgentHarness.md`
`#### Replay manifest v3 integrity matrix`, no documented procedure for staging a
byte-modified replay artifact with a repaired manifest). A later observation of
the same pair is a duplicate, not a new residual.
`Documents/Plans/Agents/ReplayChecksumLogCategory.md` also edits harness replay
documentation but covers a different symptom — per-tick frame CRC values being
compile-eliminated and therefore unscrapeable — and is not a duplicate.
