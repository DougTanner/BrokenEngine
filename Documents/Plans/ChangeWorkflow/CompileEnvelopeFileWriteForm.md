<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-24T21:19:21.464Z","dependsOn":[]} -->
# Fix: /compile worker step 9 — documented envelope-file write form is blocked by the PreToolUse hook

## Context
Observed symptom: a `/next-plan` run dispatched a `builder` for a `/compile`
Local generation build of the BrokenEngineSandbox client and server (Debug).
`.agents/skills/compile/references/worker.md` step 9 directs that the first
build's own PowerShell call create one envelope file under
`Temp/AgentBuildEnvelopes/` with a single `##` heading and print its path, and
that each later build's own call append its fenced block to that file. The
builder reported that the PreToolUse hook blocks piping a build into
`Add-Content`, so it wrote each build's envelope to its own file with
`| Set-Content`. The resulting files hold the raw `broken-engine-build-result/v1`
JSON only: no `##` heading and no fenced block. The dispatch therefore produced
two files instead of the one documented envelope file.

Step 9 asks for operations the repository's own rules do not allow in that
call. The root `AGENTS.md` `## Directives` "Bundled scripts" rule permits only
assigning the script call's output, piping it to `ConvertFrom-Json` or
`Set-Content`, or reading `$LASTEXITCODE` afterwards, and
`Test-ChainedScriptCall` (`.agents/scripts/Invoke-PreToolUseHook.ps1:678-707`,
allowlist at `:695-696`) enforces exactly that: `| Add-Content`, and any separate
statement that writes a heading or a fence, is blocked. A single `| Set-Content`
cannot add the heading or the fence.

The documented shape has consumers: `.agents/skills/compile/SKILL.md`
`## Handoff` cites the envelope file as path plus `##` selector (line 93), and
`Read-BuildEnvelopes` in
`.agents/skills/optimize-build-time/scripts/Measure-BuildTrace.ps1:137-149`
accepts only fenced `broken-engine-build-result/v1` blocks and stops with
`envelope.no-build-result` on a raw-JSON file.

Session provenance (machine-local; not reproducible after cleanup). The Client
through Worktree fields name the session that observed the friction — the
session `/next-plan-review` must reach — while the `Landing ref` line names a
ref whose tree actually contains this Plan:
- Client: claude
- Conversation session ID: 5dcbdc70-8ebe-4339-a698-b4d8b614d50e
- Worktree/branch UUID: f524889d-98ac-40da-8484-00ea2c8f9455
- Session branch: claude/f524889d-98ac-40da-8484-00ea2c8f9455
- Worktree: .claude\worktrees\BrokenEngine\f524889d-98ac-40da-8484-00ea2c8f9455
- Landing ref: claude/f524889d-98ac-40da-8484-00ea2c8f9455
  Fallback once the recorded ref is gone:
  `git log --follow --diff-filter=A --format=%H -- Documents/Plans/ChangeWorkflow/CompileEnvelopeFileWriteForm.md`,
  but a periodic Plan-history squash can make it return an unrelated aggregate
  commit, so review its result only when the commit is attributable to one
  session alone (its diff limited to that session's files); never review an
  aggregate or multi-session squash commit.
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

Author's recommendation: give `Invoke-CompileBuild.ps1` one optional parameter
naming the dispatch's envelope file; when supplied, the script itself writes
its own `broken-engine-build-result/v1` envelope into that file as one fenced
block, creating the file with the dispatch's `##` heading when it is absent,
while still printing the envelope on stdout as today. Step 9 then passes the
same literal path on every build call, and each call stays a single legal
script call. Rationale: the file shape every consumer reads stays unchanged,
and neither the root "Bundled scripts" rule nor the hook needs a new allowed
form. The rejected alternative, writing one raw-JSON file per build with
`| Set-Content`, would change the `/compile` handoff and
`Measure-BuildTrace.ps1` as well.

## Critical files
- `.agents/skills/compile/references/worker.md`
- `.agents/skills/compile/scripts/Invoke-CompileBuild.ps1`

## In scope
- Root-cause investigation as `## Design` states
- The smallest resulting fix, confined to `worker.md` step 9 (the envelope-file
  bullets, plus the step 3 sentence on the one envelope file only if its wording
  must follow) and `Invoke-CompileBuild.ps1`'s parameter block and its envelope
  output

## Out of scope
- The landed change the session produced
- The root `AGENTS.md` "Bundled scripts" rule and
  `.agents/scripts/Invoke-PreToolUseHook.ps1`
- The `broken-engine-build-result/v1` schema, `.agents/skills/compile/SKILL.md`
  `## Handoff`, and `Measure-BuildTrace.ps1`
- Unrelated skills/scripts; any transcript path or transcript text in the repo

## Risk tier and invariants
Expected Tier 2 (scoped tool behavior: one skill's build entry point and its
worker reference); escalate if the fix reaches build/bootstrap coordination,
such as WorktreeCli build serialization or the target lock. The stdout envelope
and the process exit code keep their current meaning. Never embed transcript
paths or home paths.

## Acceptance criteria
- The recorded symptom no longer reproduces under the documented invocation: a
  `/compile` dispatch that builds client and server runs every step 9 call
  without a hook block and leaves one envelope file holding one `##` heading and
  one fenced `broken-engine-build-result/v1` block per build
- `Measure-BuildTrace.ps1 -BuildEnvelope '<that file>'` reads every build's
  envelope without `envelope.no-build-result`
- The static-checks runner, invoked as `.agents/references/change-workflow.md`
  `#### Step 5 — Run targeted pre-review checks` documents it, reports every
  row the change triggers passing
