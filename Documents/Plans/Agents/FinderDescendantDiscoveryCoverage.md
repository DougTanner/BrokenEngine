<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-16T17:49:53.185Z","dependsOn":[]} -->
# Fix: Find-AgentSessionTranscript.ps1 — descendant discovery omits children of a long-running session

## Context

During a `/next-plan-review` of landing commit
`04807415125611a53b199c20ddf36f1e70a42565`, the finder was run as documented:

```powershell
pwsh -NoProfile -File .agents/skills/next-plan-review/scripts/Find-AgentSessionTranscript.ps1 -Commit 04807415125611a53b199c20ddf36f1e70a42565
```

It returned a single Codex root candidate (session
`01a00722-4f92-7202-94a6-6f26053bded7`) whose emitted `descendants` list and
`descendantCount` held 33 entries. From that same parent session's own
`spawn_agent` events and the Codex session store, the reviewer proved the parent
spawned 47 children and recovered the 14 the finder never listed, including the
`audit_plan` child, the `implement_attribution` child, and four
cpp/scope/adversarial review triples. A review that starts from the finder's
list alone therefore begins with an incomplete routing inventory and can miss
whole review rounds without any signal that the list is short.

This is not the already-disproven premise of the retired Plan
`Documents/Plans/Agents/FinderDescendantAgentPathAttribution.md`, which was
rejected because its symptom came from host-side truncation of an oversized
needs-selection result. The observation above post-dates the payload bound in
commit `d9e2a28028973ba320eb5dd52dce4c1910a3ad88`, and a non-null `descendants`
array is emitted only on the single-candidate `pass` path, so the 14 entries are
absent from the result the script itself produced. The parent session ran for
many hours before that commit, which makes the finder's bounded discovery inputs
(`.agents/skills/next-plan-review/scripts/Find-AgentSessionTranscript.ps1:340-361`,
feeding the descendant grouping at `:374-393`) the region to investigate; the
proven root cause is deliberately deferred to the review named in `## Design`.

The claimed Plan active when this was observed
(`Documents/Plans/Agents/NextPlanClaimDirtyWorktreeResume.md`) is scoped to the
`/next-plan` claim and deferral scripts, so this finder script is outside it and
is tooling friction rather than an in-scope blocker.

Session provenance (machine-local; not reproducible after cleanup):

- Client: claude
- Conversation session ID: 731a9400-fe85-40e5-aa8c-1a5a84157ee1
- Worktree/branch UUID: e6aec63b-d896-4432-a476-4da999162e12
- Session branch: claude/e6aec63b-d896-4432-a476-4da999162e12
- Worktree: .claude\worktrees\BrokenEngine\e6aec63b-d896-4432-a476-4da999162e12
- Subject of the symptom: the Codex parent session
  `01a00722-4f92-7202-94a6-6f26053bded7` and landing commit
  `04807415125611a53b199c20ddf36f1e70a42565`; that Codex session's producing
  worktree must still be registered for its transcripts to be discoverable.
- Landing ref: the session branch above, whose tip is the session's final commit
  and which survives exactly as long as the worktree recorded above. Fallback
  once that branch is gone:
  `git log --diff-filter=A --format=%H -- <this plan path>`, but a periodic
  Plan-history squash can make it return an unrelated aggregate commit, so review
  its result only when the commit is attributable to this session alone (its diff
  limited to this session's files); never review an aggregate or multi-session
  squash commit.
- Run the review before `/cleanup-worktrees` removes either worktree: Codex
  transcript discovery requires the producing worktree to remain registered, and
  Claude review requires the exact conversation session ID above.

## Design

In a new session, run
`/next-plan-review 04807415125611a53b199c20ddf36f1e70a42565`, supplying the
recorded Codex parent session and this Plan's provenance. Root-cause the missing
descendants from the proven transcripts, then make the smallest fix inside the
`## In scope` boundary below. If root-causing shows the fix lies outside that boundary, surface it for
re-planning instead of expanding scope.

## Critical files

- `.agents/skills/next-plan-review/scripts/Find-AgentSessionTranscript.ps1` —
  the bounded transcript gathering that feeds descendant discovery (`:340-361`)
  and the descendant grouping and emission blocks (`:374-393`, `:406-423`).
- `.agents/skills/next-plan-review/scripts/Test-Find-AgentSessionTranscript.ps1` —
  the existing fixture that must cover the corrected coverage behavior.

## In scope

- Root-cause investigation via `/next-plan-review` with the recorded
  provenance.
- The smallest resulting fix, confined to descendant discovery in
  `Find-AgentSessionTranscript.ps1` — the file gathering that supplies descendant
  records and the descendant grouping and emission named above — plus matching
  coverage in the existing fixture script.

## Out of scope

- Candidate selection itself: the eligible-worktree constraint, the
  commit-covering time constraint, and the needs-selection payload bound stay as
  they are.
- `/next-plan-review`'s own routing, coverage, and reporting rules beyond
  consuming a complete descendant list.
- The landed change the reviewed session produced; unrelated skills and scripts;
  any transcript path, home path, or transcript text in the repository.

## Risk tier and invariants

Expected Tier 2 (scoped tool behavior in one script and its fixture); escalate if
the fix reaches candidate selection or build/bootstrap coordination. Preserve the
schema `broken-engine-agent-session-transcript/v2` contract, the truthful
pass/needs-selection/blocked exit codes, the reparse-point and read-error safety
handling, and the bound that keeps a multi-candidate listing under host
tool-output limits. Never embed transcript paths or home paths.

## Acceptance criteria

- For a root session whose children the finder is documented to discover, the
  single-candidate result's `descendants` and `descendantCount` include every
  such child; the recorded 33-of-47 case either reports all 47 or returns a
  truthful blocked or read-error result naming the coverage limit instead of a
  silently short list.
- A short list is never reported as a complete one: any coverage the finder
  cannot guarantee is stated in the result the caller reads.
- The existing fixture script passes, and WorktreeCli `plan validate` exits `0`
  with `status: valid` and `code: ok`; `/validate-skill` passes for any changed
  `SKILL.md`.

## Notes

This Plan is keyed to the pair (`Find-AgentSessionTranscript.ps1` descendant
discovery, a single-candidate `pass` result whose descendant list omits children
the parent provably spawned). A later observation of that same pair is a
duplicate, not a new residual. The counts, session IDs, and commit hashes above
are the whole evidence record; no transcript path or transcript text is embedded.
