<!-- broken-engine-plan/v1 {"createdUtc":"2026-09-05T22:39:41.905Z","dependsOn":[]} -->
# Fix: root AGENTS.md bundled-scripts rule — no allowed form for capturing a script's exit code

## Context
During a `/next-plan` run an `/implement-plan` worker was asked to confirm the
exit code of the skill validator. The root `AGENTS.md` `## Directives`
bundled-scripts rule requires "One script per shell call with nothing chained
before or after it", and names only two allowed ways to consume a call's own
output — assigning it, or piping it to `ConvertFrom-Json` — with no form for
reading the exit code. The worker ran

```powershell
Write-Output '...'; pwsh -NoProfile -File .agents/skills/validate-skill/scripts/Validate-Skill.ps1 -Path <target> > $null; Write-Output $LASTEXITCODE
```

which chains statements around a bundled script, and self-reported the
deviation as a residual. The cost was concrete: the validator was run twice for
one verdict, and main had to adjudicate a self-reported rule deviation instead
of reading a result.

The gap is real in the current tree, not a misreading. `AGENTS.md:172` is the
whole rule and stops at the two output-consuming forms. Meanwhile
`.agents/skills/validate-skill/references/worker.md:19-27` requires exactly the
missing capability: step 2 requires "`VALID` and exit `0`", step 3 requires "the
exact command, exit status, and complete output" be captured, and step 4 makes
"a result/exit mismatch" a `BLOCKED` outcome — a comparison that cannot be made
without both values. `Find-SkillInboundReferences.ps1` consumption at
`worker.md:34-35` likewise keys a blocker on "exit `1`". So a documented skill step
requires a capture the governing rule provides no way to perform.

Session provenance (machine-local; not reproducible after cleanup). The Client
through Worktree fields name the session that observed the friction — the
session `/next-plan-review` must reach — while the `Landing ref` line names a
ref whose tree actually contains this Plan:
- Client: claude
- Conversation session ID: 48395902-6e97-4935-b2f0-8260d4caab4b
- Worktree/branch UUID: aa97c3c8-4755-4e7c-a524-272c5f68c6a1
- Session branch: claude/aa97c3c8-4755-4e7c-a524-272c5f68c6a1
- Worktree: .claude\worktrees\BrokenEngine\aa97c3c8-4755-4e7c-a524-272c5f68c6a1
- Landing ref: claude/aa97c3c8-4755-4e7c-a524-272c5f68c6a1
- Run the review before /cleanup-worktrees removes the worktree recorded above:
  Codex transcript discovery requires the producing worktree to remain
  registered, and Claude review requires the exact conversation session ID
  above.

## Design
First root-cause the friction from the current tree and this Plan's `## Context`.
Only when the transcript is genuinely needed, in a new session run
`/next-plan-review claude/aa97c3c8-4755-4e7c-a524-272c5f68c6a1` in bounded
friction mode, supplying the recorded client and conversation session ID. Then
make the smallest fix inside the `## In scope` boundary below.

Two candidate fixes, both a wording change to the one bundled-scripts rule:

1. Name reading `$LASTEXITCODE` in the same shell call as a third allowed
   result-consuming form, alongside assigning the output and piping it to
   `ConvertFrom-Json`, so one invocation yields both the output and the exit
   code.
2. State that a script's own printed result is the evidence and that the exit
   code is not separately captured, and correspondingly relax the
   `/validate-skill` steps that require an exit status.

The author's recommendation is candidate 1. It is the smaller change — one
clause in `AGENTS.md:172` and nothing else — and it keeps working the
result/exit-mismatch check that `/validate-skill` step 4 already performs, which
candidate 2 would have to delete. It is also consistent with how the repository
already treats exit codes as first-class evidence: `/compile` reports `exitCode`
per project in its result envelope and its `Decisive checks` row
(`.agents/skills/compile/SKILL.md:64`, `:84`, `:98`). The implementing session
should confirm that no other bundled-script consumer would be encouraged to
chain additional statements by the new wording, and should state the exact form
it authorizes.

For completeness the implementing session should also confirm what
`.agents/references/static-checks.md` expects, which the recording session
checked and found carries no exit-code capture wording of its own; if that
remains true it needs no edit.

If root-causing shows the fix lies outside the boundary below, surface it for
re-planning instead of expanding scope.

## Critical files
- `AGENTS.md` — the bundled-scripts rule under `## Directives` (`:172`), the
  authorized fix boundary
- `.agents/skills/validate-skill/references/worker.md` — the consumer that
  requires an exit status (`:19-27`, `:34-35`); edited only if the chosen candidate
  changes what it may require
- `.agents/references/static-checks.md` — checked for conflicting wording;
  edited only if it turns out to carry any
- `.agents/skills/compile/SKILL.md` — read-only precedent for exit-code
  reporting; not modified

## In scope
- Root-cause investigation as `## Design` states
- The smallest resulting wording fix to the bundled-scripts rule in the root
  `AGENTS.md` `## Directives` section
- Only the consumer wording that the chosen candidate makes incorrect, in the
  files named above

## Out of scope
- Every other clause of the bundled-scripts rule: the canonical
  `pwsh -NoProfile -File` form, the no-absolute-path and no-`-ExecutionPolicy
  Bypass` bans, the working-directory ban, the `Import-Module` form, and the
  array-parameter `-Command` form
- Any other root `AGENTS.md` directive or section
- Changing any script, including `Validate-Skill.ps1` and its exit codes
- The landed change the observing session produced
- Unrelated skills/scripts; any transcript path or transcript text in the repo

## Risk tier and invariants
Expected Change Workflow Tier 1, on the root `AGENTS.md` trigger
"documentation, style, project membership, or local behavior-preserving work
with no public signature or invariant exposure" — the change is instruction
prose and alters no code, script, or data. Escalate to Tier 2 if the chosen fix
turns out to require changing a script's behavior or exit codes. Invariants to
preserve: one script per shell call with nothing chained before or after it
remains the rule, and the canonical invocation form is unchanged; the fact that
this rule is stated exactly once, at its owning layer, so no skill restates it.
Never embed transcript paths or home paths.

## Acceptance criteria
- A session that must report a bundled script's exit code has one documented
  form that runs the script once and needs no self-reported rule deviation
- The `/validate-skill` steps that require an exit status are consistent with
  the rule after the change
- The rule still forbids chaining unrelated statements around a bundled script,
  and its canonical invocation form is unchanged
- `/validate-skill` passes for any changed `SKILL.md`; plan validate exits 0
