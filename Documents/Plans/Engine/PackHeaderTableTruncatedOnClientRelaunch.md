<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-24T01:08:57.574Z","dependsOn":[]} -->
# Fix: `PackChunks` lazy pack validation — intermittent `pack header table truncated` on an unchanged Texture.pack

## Context
Observed symptom: during a `/next-plan` run, relaunching the BrokenEngineSandbox
Debug client through `/agent-harness` on Shared runtime data (the client reads
the primary checkout's `Output\Data`, as
`.agents/skills/compile/references/runtime-data-mode.md` describes) failed at
startup. The client hit `DEBUG_BREAK` in `FailMissingRequiredAsset`
(`Engine/Source/File/PackChunks.cpp`) with reason `pack header table truncated`
for `Texture.pack`. That reason is raised in the lazy-pack validation loop of
the pack loader when `ReadFile` on the reopened validation handle returns
`FALSE` or reads fewer than `sizeof(common::ChunkHeader)` bytes for a chunk
header. The pack file on disk was unchanged afterwards (3200812560 bytes), and
an immediate retry of the same relaunch started cleanly. The supplied evidence
records no exact harness command sequence beyond that client relaunch.

Facts from the current code that bear on root-causing: the reason string covers
both a failed `ReadFile` and a short read, and the log line records neither
`GetLastError()` nor the byte count or chunk offset. The lazy pack is opened
`FILE_SHARE_READ` with `FILE_FLAG_NO_BUFFERING`, and the header reads go
through a `ReOpenFile` handle with no flags. The cause is unknown; whether
another process (for example a Shared-data refresh,
`runtime-data-mode.md` `## Wrapper bootstrap Shared-data refresh`) held or
rewrote the file at that moment is unverified.

Cost: the client relaunch had to be retried, and the failure was left
uninvestigated.

Session provenance (machine-local; not reproducible after cleanup). The Client
through Worktree fields name the session that observed the friction — the
session `/next-plan-review` must reach — while the `Landing ref` line names a
ref whose tree actually contains this Plan:
- Client: claude
- Conversation session ID: 4422f9e3-bfee-4a1a-abe2-268f45560966
- Worktree/branch UUID: b719c635-512a-4a63-bc40-8e90d6b34190
- Session branch: claude/b719c635-512a-4a63-bc40-8e90d6b34190
- Worktree: .claude\worktrees\BrokenEngine\b719c635-512a-4a63-bc40-8e90d6b34190
- Landing ref: claude/b719c635-512a-4a63-bc40-8e90d6b34190
  Fallback once the recorded ref is gone:
  `git log --follow --diff-filter=A --format=%H -- Documents/Plans/Engine/PackHeaderTableTruncatedOnClientRelaunch.md`, but a periodic
  Plan-history squash can make it return an unrelated aggregate commit, so
  review its result only when the commit is attributable to one session alone
  (its diff limited to that session's files); never review an aggregate or
  multi-session squash commit.
- Run the review before /cleanup-worktrees removes the worktree recorded above:
  Codex transcript discovery requires the producing worktree to remain
  registered, and Claude review requires the exact conversation session ID
  above. OpenCode transcript review remains unsupported regardless of worktree
  retention.

## Design
First root-cause the friction from the current tree and this Plan's `## Context`.
Only when the transcript is genuinely needed, in a new session run
`/next-plan-review <review ref>` in bounded friction mode — the landing ref —
supplying the recorded client and the recorded conversation session ID. Then
make the smallest fix inside the `## In scope` boundary below. If root-causing
shows the fix lies outside that boundary, surface it for re-planning instead of
expanding scope.

The failure is intermittent and its current diagnostic does not separate a
failed read from a short read. If root-causing from code alone cannot prove the
cause, the author recommends that the fix be limited to that one failure site:
report `GetLastError()` for a failed read, and the bytes read and chunk offset,
in its diagnostic, so the next occurrence names its cause. The fix session then
reports the cause as unproven rather than guessing at a fix. The trust-boundary
behavior stays: a pack that really is corrupt or truncated still fails through
`FailMissingRequiredAsset`.

## Critical files
- `Engine/Source/File/PackChunks.cpp` — the lazy-pack open, `ReOpenFile`
  validation handle, and chunk-header read loop in the pack loader, and
  `FailMissingRequiredAsset`

## In scope
- Root-cause investigation as `## Design` states
- The smallest resulting fix, confined to the lazy-pack validation open, reopen,
  and header-read code and the `pack header table truncated` failure site in
  `Engine/Source/File/PackChunks.cpp`

## Out of scope
- The landed change the session produced
- The `.pack`/`.manifest` format and its version, DataPacker, and the
  Shared-data refresh scripts; a cause found there is surfaced for re-planning
- Eager-chunk loading and the lazy-chunk streaming reads after validation
- Unrelated skills/scripts; any transcript path or transcript text in the repo

## Risk tier and invariants
Expected Tier 2 (scoped runtime behavior inside one unit at the existing
pack-file trust boundary, with the format and the trust unchanged); escalate to
Tier 3 if the fix changes what the boundary trusts, changes the pack format, or
reaches build/bootstrap coordination such as the Shared-data refresh. A corrupt
or truncated required pack must still be fatal. Never embed transcript paths or
home paths.

## Acceptance criteria
- Either the root cause is stated with evidence from code or logs and the fix
  removes it, so repeated client launches through `/agent-harness` on Shared
  runtime data start without `pack header table truncated`; or, when the cause
  cannot be proven, that failure site's diagnostic distinguishes a failed read
  (with its `GetLastError()` value) from a short read (with bytes read and the
  chunk offset), per `## Design`
- A required pack that really is truncated still fails through
  `FailMissingRequiredAsset`
- The static-checks runner, invoked as `.agents/references/change-workflow.md`
  `#### Step 5 — Run targeted pre-review checks` documents it, reports every
  row the change triggers passing
