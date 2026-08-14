<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-14T17:20:17.790Z","dependsOn":[]} -->
# Fix: next-plan SKILL.md — provenance and context sentences name sources that cannot produce the values they promise

## Context

Two sentences in `.agents/skills/next-plan/SKILL.md` tell the reader to take a
value from a source that does not carry it. Both were observed during the
`/next-plan` run that executed
`Documents/Plans/Agents/CodexReviewMetricsCompareSandbox.md`, whose approved
`## In scope` covered only `.agents/skills/codex-review` metrics-compare
sandboxing, so this skill file is outside that boundary and is tooling friction
rather than an in-scope blocker.

Symptom 1 — the recorded "session UUID" cannot locate a session.
`.agents/skills/next-plan/SKILL.md:152-160` requires a friction proposal to carry
"session UUID", and directs: "Take those values from the `Get-NextPlanContext`
result already resolved in Preconditions and selection, or from
`git branch --show-current` and `git rev-parse --show-toplevel`." Every one of
those sources yields the worktree/branch UUID and nothing else:
`.agents/skills/next-plan/scripts/NextPlanWorkflowCommon.psm1:44` sets both
`Owner` and `Session` from `$session.SessionId`, and
`.agents/scripts/AgentWorktreeSession.psm1:72` derives that `SessionId` purely by
regex from the branch name `^(?:claude|codex)/<uuid>$`. The identifier
`/next-plan-review` actually needs is the client conversation/transcript session
ID: `.agents/skills/next-plan-review/SKILL.md:52-54` requires "the exact parent
transcript/session ID from client context or the user" for Claude and forbids
guessing, `:35-41` passes `-SessionId <exact-id>` to
`.agents/skills/next-plan-review/scripts/Find-AgentSessionTranscript.ps1`, which
matches transcript files named `*-$SessionId.jsonl`
(`Find-AgentSessionTranscript.ps1:344-345`) and reads `payload.session_id` from
the transcript itself (`:224`). That the two values are different is asserted by
the suite itself:
`.agents/skills/next-plan-review/scripts/Test-Find-AgentSessionTranscript.ps1:139`
asserts "Fixture Codex SessionId must not equal the worktree UUID." Meanwhile
`.agents/skills/next-plan-review/SKILL.md:68-70` states that a friction Plan's
recorded client and session id "count as a supplied exact session id", while its
recorded worktree is selection evidence only. Cost observed in this session: the
recorded provenance carried only worktree UUIDs, so the user had to look up and
supply the conversation session IDs by hand for two `/next-plan-review` runs.

Symptom 2 — wrong helper attribution.
`.agents/skills/next-plan/SKILL.md:20-22` says to "derive the authoritative
primary, baseline, owner, and provisioned WorktreeCli from
`Get-AgentWorktreeSessionContext`". That helper returns exactly
`Worktree, Branch, SessionId, PrimaryRoot, PrimaryBranch, PrimaryTip, Baseline`
(`.agents/scripts/AgentWorktreeSession.psm1:125-128`) and has no WorktreeCli
field. The provisioned WorktreeCli path is resolved and returned only by
`Get-NextPlanContext`
(`.agents/skills/next-plan/scripts/NextPlanWorkflowCommon.psm1:41-44`), which the
same skill correctly names later at `:129-131` for
`Test-NextPlanWorkflowScripts.ps1 -Executable`. The two sentences contradict each
other, so a reader following Preconditions looks for a field that does not exist.

Session provenance (machine-local; not reproducible after cleanup):
- Client: claude
- Session: 09c9ca54-0caf-49e3-9c6a-8c71b9ff88aa
- Session branch: claude/09c9ca54-0caf-49e3-9c6a-8c71b9ff88aa
- Worktree: .claude\worktrees\BrokenEngine\09c9ca54-0caf-49e3-9c6a-8c71b9ff88aa
- Landing commit: `git log --diff-filter=A --format=%H -- Documents/Plans/Agents/NextPlanProvenanceIdentityAttribution.md`
- Run the review before /cleanup-worktrees removes this worktree: Codex
  transcript discovery requires the producing worktree to remain registered,
  and Claude review requires the exact session id above.
- The recorded `Session` value above is the worktree/branch UUID, which is the
  defect this Plan fixes; a Claude review of this Plan needs the conversation
  session id from the user.

## Design

In a new session, run `/next-plan-review <landing commit>` supplying the
recorded client and session id, root-cause the friction from the proven
transcript, then make the smallest fix inside the `## In scope` boundary below.
If root-causing shows the fix lies outside that boundary, surface it for
re-planning instead of expanding scope.

Symptom 2 is a straightforward attribution correction: split the Preconditions
sentence so each value is attributed to the helper that returns it —
primary, baseline, and owner to `Get-AgentWorktreeSessionContext`, the
provisioned WorktreeCli to `Get-NextPlanContext`.

Symptom 1 resolves one decision, which the review session settles from the
proven transcript and the two skills' current text: either (a) document a
reachable source from which the authoring session can read its own conversation
session ID and record it in the provenance block, or (b) record explicitly that
only client plus worktree/branch UUID is machine-derivable, state in
`.agents/skills/next-plan/SKILL.md` that the conversation session ID must be
obtained from client context or the user, and adjust the corresponding
expectation in `.agents/skills/next-plan-review/SKILL.md:68-70` so a friction
Plan's recorded id is no longer treated as a supplied exact session id when it
is only the worktree UUID. Whichever branch the evidence supports, both skills'
terminology must end up distinguishing the worktree/branch UUID from the
conversation session ID by name, so the two are never conflated again.

## Critical files

- `.agents/skills/next-plan/SKILL.md` — `## Preconditions and selection` and
  `## Tooling friction follow-ups`
- `.agents/skills/next-plan-review/SKILL.md` — only the friction-provenance
  expectation paragraph, and only if the resolved decision requires it

## In scope

- Root-cause investigation via `/next-plan-review` with the recorded provenance
- The smallest resulting documentation fix, confined to the two files named
  above: the Preconditions helper-attribution sentence, the friction provenance
  sentence and its provenance-block field names, and the friction-provenance
  expectation paragraph in `/next-plan-review` if the resolved decision changes it

## Out of scope

- The landed change the `CodexReviewMetricsCompareSandbox.md` session produced,
  including its claim-exit sentence, six-script trigger, and friction checkpoint
  sequence edits
- Any behavior change to `AgentWorktreeSession.psm1`,
  `NextPlanWorkflowCommon.psm1`, `Find-AgentSessionTranscript.ps1`, or any other
  script; this Plan is documentation-only unless root-causing proves otherwise,
  which is a re-planning trigger
- Unrelated skills/scripts; any transcript path or transcript text in the repo

## Risk tier and invariants

Expected Tier 2 (scoped tool behavior); escalate if the fix reaches
build/bootstrap coordination. Never embed transcript paths or home paths, and
never record an absolute worktree path in a Plan.

## Acceptance criteria

- The Preconditions sentence attributes each named value to the helper whose
  return object contains it, checked against
  `AgentWorktreeSession.psm1` and `NextPlanWorkflowCommon.psm1` as they then read
- The friction provenance instruction names a source that actually yields the
  identifier `/next-plan-review` needs, or states plainly that the identifier is
  not machine-derivable and must come from client context or the user, with
  `/next-plan-review`'s expectation matching
- `/validate-skill` passes for each changed SKILL.md; `plan validate` exits 0
