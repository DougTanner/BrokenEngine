<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-06T01:25:26.143Z","dependsOn":[]} -->
# Fix: code-style-review worker — comma-separated `-PathPrefix` under `-File` splits from the PowerShell tool

## Context
Observed symptom: the `/code-style-review` worker ran the documented inventory
invocation from `.agents/skills/code-style-review/references/worker.md:20-28`,
`pwsh -NoProfile -File .agents/scripts/Get-SessionChangeInventory.ps1 ... -Regions -PathPrefix <comma-separated prefixes>`,
from the PowerShell tool. PowerShell parsed the unquoted comma list as an array
and handed the prefixes to `pwsh -File` as separate arguments, so `-PathPrefix`
bound only the first token and the inventory came back empty. The worker
re-ran the same command through Bash with one comma-joined string — a
workaround of the documented invocation — to get the session-changed ranges.
The symptom surfaced in this session's `/code-style-review` re-check handoff.

Current tree: `-PathPrefix` and `-IncludeUntracked` are `[string[]]`
parameters of `.agents/scripts/Get-SessionChangeInventory.ps1:12-13` that the
script splits on commas itself (`:776`, `:787`), so a single comma-joined token
works under `-File`; `.agents/scripts/Find-SessionCandidates.ps1:15,165`
forwards its own `[string[]] $PathPrefix` the same way. Root `AGENTS.md`
`### Directives` "Bundled scripts" says an array parameter uses
`pwsh -NoProfile -Command "& '<path>' -Param 'a','b'"` instead of `-File`, and
`.agents/references/new-plan-file.md` `## Invocation` instead documents a
comma-separated list as "one comma-separated token". The code-style-review
worker documents neither form for `-PathPrefix` (`worker.md:28` and `:90`).
The same `-File ... -IncludeUntracked <comma-separated paths>` form appears at
`.agents/skills/code-style-review/references/worker.md:26`,
`.agents/skills/comment-review/references/worker.md:15`,
`.agents/skills/adversarial-review/SKILL.md:43`,
`.agents/skills/glsl-review/references/worker.md:11`,
`.agents/skills/progressive-disclosure-review/references/worker.md:12`, and
`.agents/skills/repo-code-review/SKILL.md:61`; whether those sites misbind the
same way is unobserved.

Session provenance (machine-local; not reproducible after cleanup). The Client
through Worktree fields name the session that observed the friction — the
session `/next-plan-review` must reach — while the `Landing ref` line names a
ref whose tree actually contains this Plan:
- Client: claude
- Conversation session ID: 14bb5086-4b15-456a-8266-8c540049aaaa
- Worktree/branch UUID: d1fadb98-cfd4-47dc-aaec-88736eaff06f
- Session branch: claude/d1fadb98-cfd4-47dc-aaec-88736eaff06f
- Worktree: .claude\worktrees\BrokenEngine\d1fadb98-cfd4-47dc-aaec-88736eaff06f
- Landing ref: claude/d1fadb98-cfd4-47dc-aaec-88736eaff06f
  Fallback once the recorded ref is gone:
  `git log --follow --diff-filter=A --format=%H -- Documents/Plans/ChangeWorkflow/CodeStyleReviewPathPrefixInvocation.md`, but a periodic
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
make the smallest fix inside the `## In scope` boundary below. If
root-causing shows the fix lies outside that boundary, surface it for
re-planning instead of expanding scope.

The author recommends documenting the list argument as one quoted
comma-separated token (for example `-PathPrefix 'a,b'`), matching
`.agents/references/new-plan-file.md` `## Invocation`, because both scripts
already split a single token on commas and the `-File` form stays unchanged;
the `-Command` array form from root `AGENTS.md` is the alternative.

## Critical files
- `.agents/skills/code-style-review/references/worker.md`
- `.agents/skills/comment-review/references/worker.md`
- `.agents/skills/adversarial-review/SKILL.md`
- `.agents/skills/glsl-review/references/worker.md`
- `.agents/skills/progressive-disclosure-review/references/worker.md`
- `.agents/skills/repo-code-review/SKILL.md`

## In scope
- Root-cause investigation as `## Design` states
- The smallest resulting fix, confined to the files named above: the
  `-PathPrefix` invocation text at `code-style-review/references/worker.md`
  steps 3 and 7, and the `-IncludeUntracked <comma-separated paths>` invocation
  text at the other named sites only when root-causing confirms they misbind
  the same way

## Out of scope
- The landed change the session produced
- `.agents/scripts/Get-SessionChangeInventory.ps1` and
  `.agents/scripts/Find-SessionCandidates.ps1` behavior
- Root `AGENTS.md` "Bundled scripts" wording
- Unrelated skills/scripts; any transcript path or transcript text in the repo

## Risk tier and invariants
Expected Tier 1 (documentation-only invocation text); escalate to Tier 2 if the
fix needs a script change. Never embed transcript paths or home paths.

## Acceptance criteria
- The recorded symptom no longer reproduces under the documented invocation:
  the documented `-PathPrefix` form, run from the PowerShell tool with two
  prefixes, returns the same inventory entries as the Bash run with one
  comma-joined string
- The static-checks runner, invoked as `.agents/references/change-workflow.md`
  `#### Step 5 — Run targeted pre-review checks` documents it, reports every
  row the change triggers passing

## Notes
- Both scripts already accept a single comma-joined token, so no script change
  is expected.
