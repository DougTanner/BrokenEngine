<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-22T15:47:25.164Z","dependsOn":[]} -->
# Fix: `.agents/scripts/Get-SessionChangeInventory.ps1` — main prints full inventory JSON with blob hashes instead of writing `-OutputPath`

## Context

Three sessions hit the same context-efficiency incident when main invoked
`.agents/scripts/Get-SessionChangeInventory.ps1` without `-OutputPath`:

- 2026-11-21, `agy/524cb3a1-b765-4407-bc2e-ade8b4f9209b`: selector
  `Get-SessionChangeInventory.ps1 -IncludeUntracked`; 5,379 chars of JSON with
  blob hashes entered main context; bounding: pass `-OutputPath` and cite the
  receipt file plus `entries` selector.
- 2026-11-22, `agy/bfc232ef-cb23-48db-a353-2bd2f14023ec`: selector
  `Get-SessionChangeInventory.ps1` without `-OutputPath`; full inventory JSON
  printed to main context; bounding: emit one `-OutputPath` receipt and pass it
  with `$.entries`.
- 2026-11-22, `agy/4004df43-5824-4718-822f-007c9e5d0963`: selector
  `Get-SessionChangeInventory.ps1 -IncludeUntracked`; 5,750 chars of JSON
  entered main context; bounding: pass `-OutputPath` and give the reviewer the
  JSON path plus `entries` selector.

Current tree observations:

- `.agents/scripts/Get-SessionChangeInventory.ps1:19` declares `-OutputPath` as
  an optional string; when omitted, `Complete-SessionChangeInventory` (`:98-123`)
  writes the full JSON document — including per-entry `baseline`/`current`
  SHA-256 blob hashes (`:943-950`) — to stdout (`:121`), whereas passing
  `-OutputPath` writes the JSON to the file and emits one summary line on stdout
  (`:102-105`).
- `.agents/references/subagent-reporting.md:81-96` and
  `.agents/skills/coherence-review/SKILL.md:43` do not direct main to pass
  `-OutputPath` when gathering changed files for reviewer dispatches, unlike
  `.agents/skills/adversarial-review/SKILL.md:38` and
  `.agents/skills/finalize-changes/references/landing-acceptance-table.md:8-12`.
- Programmatic callers (`.agents/scripts/Get-ChangeWorkflowSteps.ps1:50`,
  `.agents/scripts/Find-SessionCandidates.ps1:29`,
  `.agents/scripts/Invoke-StaticChecks.ps1:47`) currently consume the script's
  JSON stdout directly.

Session provenance (machine-local; not reproducible after cleanup). The Client
through Worktree fields name the session that observed the friction — the
session `/next-plan-review` must reach — while the `Landing ref` line names a
ref whose tree actually contains this Plan:
- Client: agy
- Conversation session ID: 1630c14b-757d-461e-9e76-76ddeaeea889
- Worktree/branch UUID: 4004df43-5824-4718-822f-007c9e5d0963
- Session branch: agy/4004df43-5824-4718-822f-007c9e5d0963
- Worktree: .agy\worktrees\BrokenEngine\4004df43-5824-4718-822f-007c9e5d0963
- Landing ref: agy/4004df43-5824-4718-822f-007c9e5d0963
  The review ref the invoker passes to `/next-plan-review` has two cases. When
  the `Observed in an earlier session` line is present, it is that line's
  landed commit hash, with no lookup. When that line is absent, the recording
  session is the observing session, and worktree reuse moves its branch past
  its landing, so the invoker never reviews the ref itself: it resolves the ref
  to the commit that added this Plan with
  `git log --follow --diff-filter=A -n 1 --format=%H <ref> -- <this plan path>`
  and passes that commit. When the recorded ref is gone or that command prints
  nothing (a later session moved this Plan), it runs the same command with
  `<ref>` omitted. A periodic Plan-history squash can make either command
  return an unrelated aggregate commit, and a landing that also deletes a
  similar file can make it return another session's commit, so review the
  result only when the commit is attributable to the observing session recorded
  above alone (its diff limited to that session's files); never review an
  aggregate or multi-session squash commit.
- Run the review before `/cleanup-worktrees` removes the worktree recorded
  above: Codex discovery requires a registered worktree, and Claude or agy
  review requires the conversation session ID above.

## Design

First root-cause the friction from the current tree and this Plan's
`## Context` — specifically whether the fix belongs in
`Get-SessionChangeInventory.ps1` (for example requiring `-OutputPath` or adding
a compact/stdout switch so an interactive shell run cannot dump blob-hashed
JSON to stdout by default, while keeping programmatic script callers intact),
in the dispatch references/skills (`.agents/references/subagent-reporting.md`,
`.agents/skills/coherence-review/SKILL.md`), or both. Only when the transcript
is genuinely needed, in a new session run `/next-plan-review <review ref>` in
bounded friction mode, with the review ref the provenance block above defines,
supplying the recorded client (`agy`) and conversation session ID
(`1630c14b-757d-461e-9e76-76ddeaeea889`). Then make the smallest fix inside the
`## In scope` boundary below. If root-causing shows the fix lies outside that
boundary, surface it for re-planning instead of expanding scope.

## Critical files

- `.agents/scripts/Get-SessionChangeInventory.ps1` — parameters (`:8-20`),
  `Complete-SessionChangeInventory` (`:98-123`), entry emission (`:938-951`),
  and stdout output path (`:988-1031`).
- `.agents/references/subagent-reporting.md` — `## Task brief` inventory
  dispatch guidance (`:81-96`).
- `.agents/skills/coherence-review/SKILL.md` — `## Inputs` changed-files input
  contract (`:38-50`).
- Existing script and skill callers of `Get-SessionChangeInventory.ps1` (update
  in lockstep only if the script CLI or stdout contract changes):
  `.agents/scripts/Get-ChangeWorkflowSteps.ps1`,
  `.agents/scripts/Find-SessionCandidates.ps1`,
  `.agents/scripts/Invoke-StaticChecks.ps1`,
  `.agents/skills/adversarial-review/SKILL.md`,
  `.agents/skills/session-audit/SKILL.md`,
  `.agents/skills/finalize-changes/references/landing-acceptance-table.md`,
  `.agents/skills/code-style-review/references/worker.md`,
  `.agents/skills/comment-review/references/worker.md`,
  `.agents/skills/glsl-review/references/worker.md`,
  `.agents/skills/over-engineering-review/references/worker.md`,
  `.agents/skills/progressive-disclosure-review/references/worker.md`, and
  `.agents/skills/update-affected-code/references/worker.md`.

## In scope

- Root-cause investigation as `## Design` states.
- The smallest resulting fix, confined to
  `.agents/scripts/Get-SessionChangeInventory.ps1`,
  `.agents/references/subagent-reporting.md`,
  `.agents/skills/coherence-review/SKILL.md`, and any existing caller listed in
  `## Critical files` that must update in lockstep with a script CLI or output
  change.

## Out of scope

- The landed change the observing session produced.
- Unrelated skills/scripts, C++ or shader code, Visual Studio project files,
  and any transcript path or transcript text in the repo.
- Changing `-EmitTargets` or `-EmitManifest` schemas or target-corpus rules
  beyond preserving compatibility.

## Risk tier and invariants

Expected Tier 2 (scoped tool behavior — `.agents/scripts/Get-SessionChangeInventory.ps1`
and its workflow/skill callers; no determinism/CRC, wire, serialization,
`.pack`/`kiVersion`, replay, affinity, threading, allocation, shader, build, or
live `/agent-harness` exposure); escalate if the fix reaches build/bootstrap
coordination. Never embed transcript paths or home paths.

## Acceptance criteria

- The recorded symptom no longer reproduces under the documented invocation:
  main's inventory invocation for reviewer dispatch writes full inventory JSON
  to `-OutputPath` (or emits only a bounded summary on stdout) instead of
  printing full inventory JSON with SHA-256 blob hashes to main context.
- Programmatic callers (`.agents/scripts/Get-ChangeWorkflowSteps.ps1`,
  `.agents/scripts/Find-SessionCandidates.ps1`,
  `.agents/scripts/Invoke-StaticChecks.ps1`) continue to pass against the
  session baseline.
- The static-checks runner, invoked as `.agents/references/change-workflow.md`
  `#### Step 5 — Run targeted pre-review checks` documents it, reports every
  row the change triggers passing.

## Notes

- Live `/agent-harness` verification does not apply.
- No live Plan dependencies or reciprocal `## Coordination` constraints apply.
