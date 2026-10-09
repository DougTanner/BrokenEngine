<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-08T22:15:12.384Z","dependsOn":[]} -->
# Fix: Invoke-CompileBuild.ps1 — the Shared shader gate lists every remedy instead of the one that applies

## Context
Observed symptom: `pwsh -NoProfile -File .agents/skills/compile/scripts/Invoke-CompileBuild.ps1` for the client target in Shared data mode blocked with `data-mode.shared-shader-unverified`. The block message ends in the fixed `$remedies` string (`.agents/skills/compile/scripts/Invoke-CompileBuild.ps1:300`, used by both `Stop-CompileInvoke` calls at `:302` and `:305`). That string lists all three remedies — start a new session, rebase onto primary main, or rerun with Local generation (`-DataBuildMode Local -RunDataPacker`) — each behind a condition the script never evaluates. The gate compares the stamp only with the worktree `HEAD` shader trees (`:296`, `:304`), never with primary main's.

In the observed run, the primary's Shared shader stamp (Engine shaders tree `d517c9a4`) matched neither the worktree `HEAD` tree (`bccb4f63`) nor primary main's tree (`78c266e0`), so rebasing onto main could not clear the block. The builder returned rebase as the only remedy. Rework it forced: a user approval round, a temporary commit and a rebase, a rebuild that blocked again, a second user decision, and then about 1 GB of Local data generation. The main session also had to Grep `Invoke-CompileBuild.ps1` and read `.agents/skills/compile/references/runtime-data-mode.md` itself to work out which remedy applied, which the builder should have returned with its cost. `runtime-data-mode.md` `## Wrapper bootstrap Shared-data refresh` documents the same three-way remedy without the comparison against main.

Session provenance (machine-local; not reproducible after cleanup). The Client through Worktree fields name the session that observed the friction — the session `/next-plan-review` must reach — while the `Landing ref` line names a ref whose tree actually contains this Plan:
- Client: claude
- Conversation session ID: e4cd04e3-00be-4fbc-a391-f5696e2f03ad
- Worktree/branch UUID: 2363f492-336e-42f1-90f7-0ea80f7634c0
- Session branch: claude/2363f492-336e-42f1-90f7-0ea80f7634c0
- Worktree: .claude\worktrees\BrokenEngine\2363f492-336e-42f1-90f7-0ea80f7634c0
- Landing ref: claude/2363f492-336e-42f1-90f7-0ea80f7634c0, the session branch above, whose tip is that session's final commit and which survives exactly as long as the worktree recorded above.
  Fallback once the recorded ref is gone:
  `git log --follow --diff-filter=A --format=%H -- Documents/Plans/ChangeWorkflow/FixSharedShaderGateRemedy.md`, but a periodic Plan-history squash can make it return an unrelated aggregate commit, so review its result only when the commit is attributable to one session alone (its diff limited to that session's files); never review an aggregate or multi-session squash commit.
- Run the review before /cleanup-worktrees removes the worktree recorded above: Codex transcript discovery requires the producing worktree to remain registered, and Claude review requires the exact conversation session ID above. OpenCode transcript review remains unsupported regardless of worktree retention.

## Design
First root-cause the friction from the current tree and this Plan's `## Context`. Only when the transcript is genuinely needed, in a new session run `/next-plan-review <review ref>` in bounded friction mode — the landing ref — supplying the recorded client and the recorded conversation session ID. Then make the smallest fix inside the `## In scope` boundary below. If root-causing shows the fix lies outside that boundary, surface it for re-planning instead of expanding scope.

The author's recommendation, chosen because one comparison removes both the wrong-remedy rework and the main session's own reading: when the stamp differs from `HEAD`, have the gate also read primary main's tree hashes for the same two shader directories and name only the remedy that clears the block, with its cost. For example, if the stamp equals main, rebase clears it unless the worktree's own commits change shaders. If the stamp differs from main, a new session start must refresh the Shared data first, and a rebase is needed afterwards only if `HEAD` also differs from main. If the worktree carries shader changes main lacks, only Local generation clears it, at the cost of about 1 GB of generated data. A builder that relays the block message then returns the applicable remedy without a further read.

## Critical files
- `.agents/skills/compile/scripts/Invoke-CompileBuild.ps1`
- `.agents/skills/compile/references/runtime-data-mode.md`

## In scope
- Root-cause investigation as `## Design` states
- The smallest resulting fix, confined to the files named above: the Shared Client shader-provenance block in `Invoke-CompileBuild.ps1` (the `$remedies` string and the two `data-mode.shared-shader-unverified` stops, `:293-307`) and the remedy sentence in `runtime-data-mode.md` `## Wrapper bootstrap Shared-data refresh`

## Out of scope
- The landed change the session produced
- The bootstrap stamp writer in `.agents/scripts/Bootstrap-AgentTools.ps1`, the stamp's format, and whether the gate blocks; only the remedy it names changes
- Unrelated skills/scripts; any transcript path or transcript text in the repo

## Risk tier and invariants
Expected Tier 2. Trigger: scoped tool behavior — the remedy diagnostics of one per-session build gate, with the stamp, its writer, and the block decision unchanged. Escalate to Tier 3 if the fix reaches the bootstrap stamp writer or other build/bootstrap coordination that can block other sessions. Never embed transcript paths or home paths.

## Acceptance criteria
- The recorded symptom no longer reproduces under the documented invocation: when the stamp matches neither `HEAD` nor primary main, the block message does not offer rebase alone as the remedy, and every block message names only the remedy that applies, with its cost
- The static-checks runner, invoked as `.agents/references/change-workflow.md` `#### Step 5 — Run targeted pre-review checks` documents it, reports every row the change triggers passing
