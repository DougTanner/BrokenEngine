<!-- broken-engine-plan/v2 {"createdUtc":"2026-11-27T02:33:56.748Z","dependsOn":[]} -->
# Fix: Invoke-SubdirectoryAgentsHook.ps1 — skip AGENTS.md injection for git diff Temp/ patch output

## Context

Three distinct session branches observed
`.agents/scripts/Invoke-SubdirectoryAgentsHook.ps1` injecting C++ and source
directory `AGENTS.md` files into the main session when `git diff` wrote a patch
file under `Temp/` for a Codex reviewer:

- 2026-11-26 (`agy/8acc5298-fb63-452c-9a04-e82067273433`): selector `git diff`
  to `Temp/code-review-diff.patch` and `Temp/session-cpp-diff.patch`; observed:
  C++ `AGENTS.md` files injected into main (49,276 chars, then 4 ancestor docs);
  bounding: skip hook injection for reviewer-only patches written under `Temp/`.
- 2026-11-26 (`agy/4b97191b-8d41-4c51-85ec-f11a415112ae`): selector
  `git diff --output=Temp/session-diff.patch`; observed: injected 9
  source-directory `AGENTS.md` files into main; bounding: keep diff receipt in
  main and let reviewers load source docs.
- 2026-11-26 (`agy/0af11ee1-d402-4c18-a665-00b5ce3d1efe`): selector `git diff`
  redirected to `Temp/diff-repo-code-review-2.patch`; observed: ancestor
  `AGENTS.md` bodies injected into main; bounding: omit ancestor `AGENTS.md`
  bodies for a diff redirected solely to a reviewer artifact and pass their
  paths to the reviewer.

Verified in the current tree: in
`.agents/scripts/Invoke-SubdirectoryAgentsHook.ps1`, `Get-ShellAccessedTargets`
(lines 895-969) skips only `$script:SkipOutputCommands` and commands exempted by
`Test-IsMetadataLookupElement` (lines 527-893). Neither exempts `git diff` when
its output is written solely to a `Temp/` file (via `--output=<Temp/...>`,
`--output <Temp/...>`, or stdout shell redirection to `Temp/...`). In addition,
`.agents/scripts/AgentShellLexer.psm1` (`ShellLexer.ReadTokens`, lines 95-102,
163-166) sets `ShellWord.IsRedirectTarget` without preserving the redirect
operator or stream number, so `>` and `2>` produce identical tokens and
`IsRedirectTarget` alone cannot prove stdout redirection. As a result,
`Get-ShellAccessedTargets` (lines 944-965) scans the tracked file operands
passed to `git diff`, resolves them as existing files under the worktree, and
`Add-ShellCommandTargets` (lines 971-1016, calling `Add-AccessedTarget` at line
1013) calls `Add-AccessedTarget` and `Add-AncestorCandidates` to inject every
unseen ancestor `AGENTS.md`.

Session provenance (machine-local; not reproducible after cleanup). The Client
through Worktree fields name the session that observed the friction — the
session `/next-plan-review` must reach — while the `Landing ref` line names a
ref whose tree actually contains this Plan:
- Client: agy
- Conversation session ID: c8ee0c30-fa2c-4343-aaa9-2484b23f80fd
- Worktree/branch UUID: 0af11ee1-d402-4c18-a665-00b5ce3d1efe
- Session branch: agy/0af11ee1-d402-4c18-a665-00b5ce3d1efe
- Worktree: .agy/worktrees/BrokenEngine/0af11ee1-d402-4c18-a665-00b5ce3d1efe
- Landing ref: agy/0af11ee1-d402-4c18-a665-00b5ce3d1efe
  The review ref the invoker passes to /next-plan-review has two cases. When
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
- Run the review before /cleanup-worktrees removes the worktree recorded above:
  Codex discovery requires a registered worktree, and Claude or agy review
  requires the conversation session ID above.

## Design

First confirm the root cause from the current tree and this Plan's
`## Context`. Only when the transcript is genuinely needed, in a new session
run `/next-plan-review <review ref>` in bounded friction mode, with the review
ref the provenance block above defines, supplying the recorded client and, on
Claude or agy, the recorded conversation session ID; a Codex review supplies
the client and that review ref only. Then make the smallest fix inside the
`## In scope` boundary below. Author's recommendation: expose stream-specific
redirect metadata on `ShellWord` in `.agents/scripts/AgentShellLexer.psm1`
(lines 6-16, 95-102, 163-166) so stdout redirection (`>`, `>>`, `1>`, `1>>`,
`*>`) is distinguishable from stderr (`2>`) and stdin (`<`), and add a small
helper in `.agents/scripts/Invoke-SubdirectoryAgentsHook.ps1` invoked from
`Get-ShellAccessedTargets` (or `Test-IsMetadataLookupElement`) that detects a
`git diff` element whose diff output is written solely to a literal path under
`Temp/` — requiring either `--output=<Temp/...>`, `--output <Temp/...>`, or
stream-specific stdout redirection owned by that same `git diff` element to a
`Temp/` path with no downstream stdout-reading pipeline stage and no other
element in the same shell call reading or referencing that `Temp/` output path
— and skips collecting path targets for that element alone. Rationale: writing
a diff patch to `Temp/` for a reviewer does not bring tracked file contents
into the calling session, whereas keeping path scanning active for unredirected
`git diff`, stderr-only redirection (`2> Temp/...`), diffs written outside
`Temp/`, a same-call read of the `Temp/` patch file (e.g.,
`git diff ... > Temp/foo.patch; Get-Content Temp/foo.patch`, since
`Get-SafeAgentsCandidate` excludes `Temp/`), and any other command in the same
shell call (including when another command owns the `Temp/` redirect) preserves
ancestor `AGENTS.md` injection whenever source content actually enters the
session. If root-causing shows the fix lies outside that boundary, surface it
for re-planning instead of expanding scope.

## Critical files

- `.agents/scripts/Invoke-SubdirectoryAgentsHook.ps1`
- `.agents/scripts/AgentShellLexer.psm1`

## In scope

- Root-cause confirmation as `## Design` states
- The smallest resulting fix, confined to
  `.agents/scripts/AgentShellLexer.psm1` (`ShellWord` and
  `ShellLexer.ReadTokens`, lines 6-16, 95-102, 163-166, for stream-specific
  redirect metadata) and `.agents/scripts/Invoke-SubdirectoryAgentsHook.ps1`
  (`Get-ShellAccessedTargets`, lines 895-969, `Test-IsMetadataLookupElement`,
  lines 527-893, and any small local helper needed to recognize a `git diff`
  element writing stdout solely to a `Temp/` path that no other element in the
  same shell call reads)

## Out of scope

- The landed change the session produced
- Unrelated skills/scripts; any transcript path or transcript text in the repo

## Risk tier and invariants

Expected Tier 2 (scoped tool behavior in the subdirectory `AGENTS.md` hook);
escalate if the fix reaches build/bootstrap coordination. Invariants:
unredirected `git diff` printing to stdout, `git diff` with stderr-only
redirection (`2> Temp/...`), `git diff` writing to `Temp/` when another command
in the same shell call reads that `Temp/` patch file (e.g.,
`git diff ... > Temp/<file>.patch; Get-Content Temp/<file>.patch`), `git diff`
in a shell call where another command owns the `Temp/` redirect, `git diff`
writing outside `Temp/`, and direct reads or edits of tracked files still
inject unseen ancestor `AGENTS.md` files; never embed transcript paths or home
paths.

## Acceptance criteria

- Running `git diff` with `--output=Temp/<file>.patch`,
  `--output Temp/<file>.patch`, or shell stdout redirection
  `> Temp/<file>.patch` over tracked source files no longer causes
  `Invoke-SubdirectoryAgentsHook.ps1` to emit ancestor `AGENTS.md` bodies.
- Running an unredirected `git diff`, `git diff ... 2> Temp/<file>.log`
  (stderr-only redirection), a same-call read of the written patch
  (`git diff ... > Temp/<file>.patch; Get-Content Temp/<file>.patch`), or a
  multi-command shell call where another command owns the `Temp/` redirect over
  tracked source files (or reading those files directly) still causes
  `Invoke-SubdirectoryAgentsHook.ps1` to emit unseen ancestor `AGENTS.md` files.
- The static-checks runner, invoked as `.agents/references/change-workflow.md`
  `#### Step 5 — Run targeted pre-review checks` documents it, reports every
  row the change triggers passing.

## Notes

- Converted from three incident rows in
  `Documents/Investigations/ChangeWorkflow/WorkflowIncidents.md` for key
  `(.agents/scripts/Invoke-SubdirectoryAgentsHook.ps1, hook injects C++`
  `` `AGENTS.md` `` `files into main when` `` `git diff` `` `writes a patch`
  `file for a Codex reviewer)`.
