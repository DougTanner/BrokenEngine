<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-15T16:10:08.090Z","dependsOn":[]} -->
# Fix: agent-harness claim/launch — a binaries-vs-shared-data pack version mismatch surfaces only as a hidden modal dialog and a ping timeout

## Context
A wrapper worktree built at baseline `8a00a60` produced Debug client and server
binaries whose `common::DataHeader::kiVersion` (`Common/DataFile.h:432`,
`51 + sizeof(ChunkHeader)`) evaluated to `426`, while the shared
`Output\Data` tree the harness launches against had already been re-exported
from primary (`f1090e4`) at version `427`. The manifests on disk carried magic
`0xDA7AF11E` and version `427`.

`pwsh -NoProfile -File .agents/skills/agent-harness/scripts/Invoke-HarnessClaim.ps1`
returned a passing claim result (`2026-08-15T15:15:39.671Z`) and both launches
started, so nothing in the claim or launch path reported the mismatch. Each
process instead hit the manifest trust boundary in
`Engine/Source/File/PackChunks.cpp:171-173`, which calls
`FailMissingRequiredAsset(manifestPath, "manifest header missing or version mismatch")`.
That failure presented as a hidden modal Win32 dialog on a minimized agent
window, so the process neither exited nor wrote a log file, and
`.agents/skills/agent-harness/scripts/Wait-HarnessPing.ps1` returned
`ping.timeout` after 53 attempts / 120016 ms with no diagnostic content.
Diagnosing it required reading the dialog text out of the Win32 window directly
— a step no skill documents and that a timeout result gives no reason to try.

The claimed active intent was `Documents/Plans/Frame/TerrainTraceToEngine.md`,
whose `## In scope` covered only moving `SegmentHit`,
`TracePointAgainstTerrain`, and `TracePointToFrameExit` and requalifying their
call sites; the harness claim, launch, and wait scripts are outside that
boundary, so this is `/next-plan` tooling friction rather than an in-scope
failure of the active change. The exact root cause is intentionally deferred to
`/next-plan-review`.

Session provenance (machine-local; not reproducible after cleanup):
- Client: claude
- Conversation session ID: ccaa8c39-19a7-4b6c-b2b6-3b6fb1bb0de1
- Worktree/branch UUID: ec7d0a29-fe8c-4d31-b355-17406342ca2e
- Session branch: claude/ec7d0a29-fe8c-4d31-b355-17406342ca2e
- Worktree: .claude\worktrees\BrokenEngine\ec7d0a29-fe8c-4d31-b355-17406342ca2e
- Landing ref: the session branch above, whose tip is the session's final
  commit and which survives exactly as long as the worktree recorded above.
  Fallback once that branch is gone:
  `git log --diff-filter=A --format=%H -- <this plan path>`, but a periodic
  Plan-history squash can make it return an unrelated aggregate commit, so
  review its result only when the commit is attributable to this session alone
  (its diff limited to this session's files); never review an aggregate or
  multi-session squash commit.
- Run the review before /cleanup-worktrees removes this worktree: Codex
  transcript discovery requires the producing worktree to remain registered,
  and Claude review requires the exact conversation session ID above.

## Design
In a new session, run `/next-plan-review <landing ref>` supplying the recorded
client and the recorded conversation session ID; a Codex review supplies the
client and landing ref only. Root-cause the friction from the proven transcript,
then make the smallest fix inside the `## In scope` boundary below: the harness
claim/launch path detects that the binaries it is about to launch expect a
different pack version than the shared data on disk carries, and reports that as
a structured blocked result naming both versions instead of launching and
letting the wait script time out. If root-causing shows the fix lies outside
that boundary — for example in the compile receipt chain or the shared data
export — surface it for re-planning instead of expanding scope.

## Critical files
- `.agents/skills/agent-harness/scripts/Invoke-HarnessClaim.ps1` — the claim and
  launch path that currently passes with mismatched data
- `.agents/skills/agent-harness/scripts/Wait-HarnessPing.ps1` — the wait whose
  `ping.timeout` was the only signal
- `.agents/skills/agent-harness/SKILL.md` — the claim/launch contract and its
  blocked-result codes

## In scope
- Root-cause investigation via /next-plan-review, run with the recorded landing
  ref and client, plus, on Claude, the recorded conversation session ID; a Codex
  review supplies the client and landing ref only
- The smallest resulting fix, confined to the files named above: a pre-launch
  pack version pairing check and its structured blocked result, plus the
  matching skill documentation

## Out of scope
- The landed change the session produced
- `Common/DataFile.h`, `Engine/Source/File/PackChunks.cpp`, and the runtime
  manifest trust boundary itself, which must keep failing loud
- The Win32 modal-dialog behavior of a failing launch, and the shared data
  export and build/bootstrap coordination that produced the mismatch
- Unrelated skills/scripts; any transcript path or transcript text in the repo

## Risk tier and invariants
Expected Tier 2 (scoped tool behavior); escalate if the fix reaches
build/bootstrap coordination or the shared data export. A correctly paired
launch must still proceed unchanged, and the check must not itself launch or
mutate shared data. Never embed transcript paths or home paths.

## Acceptance criteria
- With binaries expecting one pack version and the shared data carrying
  another, the claim/launch path returns a structured blocked result naming both
  versions instead of launching, and no ping wait is entered
- With binaries and shared data at the same pack version, claim and launch
  behave exactly as they do today
- /validate-skill passes for any changed SKILL.md; plan validate exits 0

## Notes
This Plan is keyed to the pair (agent-harness claim/launch path, launch against
shared data whose pack version the binaries do not accept, observed only as a
hidden modal dialog plus `ping.timeout`). A later observation of the same pair
is a duplicate, not a new residual. This body records the observed symptom, the
version values, and the forced dialog-text diagnosis without embedding
transcript material.
