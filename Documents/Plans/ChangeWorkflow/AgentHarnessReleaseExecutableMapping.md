<!-- broken-engine-plan/v1 {"createdUtc":"2026-09-06T19:58:34.321Z","dependsOn":[]} -->
# Fix: Invoke-HarnessClaim.ps1 — Release claims require nonexistent suffixed executables

## Context
Running `.agents/skills/agent-harness/scripts/Invoke-HarnessClaim.ps1` for a client/server `Release` claim returned `claim.executable-missing` for `BrokenEngineSandboxServer.Release.exe` and `BrokenEngineSandbox.Release.exe` (`Temp/PackChunkLoader-harness-claim.json`). Both Release builds had succeeded (`Temp/AgentBuildEnvelopes/compile-20260906T194305470Z-25924.md`), and the output directory instead contained the canonical unsuffixed `BrokenEngineSandboxServer.exe` and `BrokenEngineSandbox.exe`.

The claim script reads the documented Debug executable names and replaces `.Debug.exe` with `.$Configuration.exe` at `.agents/skills/agent-harness/scripts/Invoke-HarnessClaim.ps1:234-264`. That produces the nonexistent `.Release.exe` names even though both Release project configurations set `TargetName` to `$(ProjectName)` (`Projects/BrokenEngineSandbox/Platforms/VisualStudio2026/BrokenEngineSandbox.vcxproj:97-100`, `Projects/BrokenEngineSandbox/Platforms/VisualStudio2026/BrokenEngineSandboxServer.vcxproj:95-98`). The same script and project behavior exist at the session baseline, so this friction predates the loader extraction whose runtime verification exposed it. The supported workaround was to build and run matching Debug executables; this did not settle the Release runtime presentation.

Session provenance (machine-local; not reproducible after cleanup). The Client through Worktree fields name the session that observed the friction — the session `/next-plan-review` must reach — while the `Landing ref` line names a ref whose tree actually contains this Plan:
- Client: codex
- Conversation session ID: none
- Worktree/branch UUID: 005e0c35-3f1e-4ceb-ab64-bb72a32b3073
- Session branch: codex/005e0c35-3f1e-4ceb-ab64-bb72a32b3073
- Worktree: .codex\worktrees\BrokenEngine\005e0c35-3f1e-4ceb-ab64-bb72a32b3073
- Landing ref: codex/005e0c35-3f1e-4ceb-ab64-bb72a32b3073
  Fallback once the recorded ref is gone:
  `git log --follow --diff-filter=A --format=%H -- Documents/Plans/ChangeWorkflow/AgentHarnessReleaseExecutableMapping.md`, but a periodic Plan-history squash can make it return an unrelated aggregate commit, so review its result only when the commit is attributable to one session alone (its diff limited to that session's files); never review an aggregate or multi-session squash commit.
- Run the review before /cleanup-worktrees removes the worktree recorded above: Codex transcript discovery requires the producing worktree to remain registered.

## Design
First root-cause the friction from the current tree and this Plan's `## Context`. Only when the transcript is genuinely needed, in a new session run `/next-plan-review codex/005e0c35-3f1e-4ceb-ab64-bb72a32b3073` in bounded friction mode, supplying the recorded client and review ref. Then make the smallest fix inside the `## In scope` boundary below. If root-causing shows the fix lies outside that boundary, surface it for re-planning instead of expanding scope.

The author's recommendation is to make the claim script map the documented Debug names to the established build output matrix: keep `.Debug.exe` for Debug, use `.Profile.exe` for Profile, and remove the configuration suffix for Release. This preserves the project files and the documentation hub as the output-name authorities while changing only the faulty consumer mapping.

## Critical files
- `.agents/skills/agent-harness/scripts/Invoke-HarnessClaim.ps1:234-264` — executable-name mapping and preflight existence check

## In scope
- Root-cause investigation as `## Design` states
- The smallest resulting fix, confined to the executable-name mapping and preflight existence check in `.agents/skills/agent-harness/scripts/Invoke-HarnessClaim.ps1:234-264`

## Out of scope
- The loader extraction and its runtime acceptance evidence
- Visual Studio project output names, the documented launch configuration, and runtime client/server behavior
- Unrelated skills or scripts; any transcript path or transcript text in the repository

## Risk tier and invariants
Expected Tier 2 because this changes scoped agent-harness tool behavior. Escalate if the fix reaches build/bootstrap coordination. Preserve the canonical unsuffixed Release outputs and the existing suffixed Debug and Profile outputs. Never embed transcript paths or home paths.

## Acceptance criteria
- With canonical client and server Release outputs present, a documented `Invoke-HarnessClaim.ps1 -Configuration Release` invocation resolves the unsuffixed executables and succeeds instead of returning `claim.executable-missing`; release the resulting claim through the documented route.
- Equivalent Debug and Profile claims continue to resolve their `.Debug.exe` and `.Profile.exe` executables.
- The Visual Studio target names and `Projects/BrokenEngineSandbox/Documents/AgentHarness.md` launch assignments remain unchanged.
- `/validate-skill` passes for the agent-harness skill package, and plan validation exits 0.

## Coordination
No dependencies or reciprocal coordination constraints.

## Notes
This Plan records pre-existing tooling friction only. Its creation does not satisfy or waive the runtime acceptance criteria that exposed the mismatch.
