<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-15T15:49:36.436Z","dependsOn":[]} -->
# Fix: compile / WorktreeCli build — documented absolute solution target is rejected as nonexistent

## Context

The documented full-build commands in `.agents/skills/compile/SKILL.md:118-120`
pass `$ROOT\Projects\BrokenEngineSandbox\Platforms\VisualStudio2026\BrokenEngineSandbox.sln`
and the corresponding server solution as absolute targets:

```powershell
& $WorktreeCli build "$ROOT\Projects\BrokenEngineSandbox\Platforms\VisualStudio2026\BrokenEngineSandbox.sln" '/p:Configuration=Debug' '/p:Platform=x64' @DataProperties '/p:EnableClangTidyCodeAnalysis=false' '/p:RunCodeAnalysis=false' '/verbosity:minimal'
& $WorktreeCli build "$ROOT\Projects\BrokenEngineSandbox\Platforms\VisualStudio2026\BrokenEngineSandboxServer.sln" '/p:Configuration=Debug' '/p:Platform=x64' @DataProperties '/p:EnableClangTidyCodeAnalysis=false' '/p:RunCodeAnalysis=false' '/verbosity:minimal'
```

During this session, the provisioned WorktreeCli run of that documented
absolute-target form returned the typed `broken-engine-build-result/v1` failure
with `status:fail`, `failureKind:tool`, message `build target does not exist`,
and `msbuild.launched:false`. Repeating the same client and server builds with
the equivalent repository-relative targets under
`Projects\BrokenEngineSandbox\Platforms\VisualStudio2026\` succeeded. The
absolute-target failure therefore forced replacement build invocations and
repeated the build/oracle workflow instead of reaching MSBuild.

The target is parsed and checked before MSBuild at
`Tools/WorktreeCli/BuildCommand.cpp:807-832`; the same function emits the
requested and normalized target identities at `:814`. The compile skill's
documented target form and this WorktreeCli path boundary are outside the
claimed `Documents/Plans/Engine/UiStateHoistToGameBase.md` boundary, which is
limited to UiState/GameBase C++ and ownership documentation. Runtime UI
behavior passed functionally, so this is tooling friction rather than an
in-scope UI acceptance failure.

Session provenance (machine-local; not reproducible after cleanup):
- Client: codex
- Conversation session ID: none
- Worktree/branch UUID: f21d89f0-d1a2-48c5-ab51-8d31abef1593
- Session branch: codex/f21d89f0-d1a2-48c5-ab51-8d31abef1593
- Worktree: .codex\worktrees\BrokenEngine\f21d89f0-d1a2-48c5-ab51-8d31abef1593
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
client and landing ref only. Root-cause the friction from the proven
transcript, then make the smallest fix inside the `## In scope` boundary below.
If root-causing shows the fix lies outside that boundary, surface it for
re-planning instead of expanding scope.

## Critical files

- `.agents/skills/compile/SKILL.md` — full client/server target invocations and
  the structured build-result contract (`:14-23`, `:101-120`).
- `Tools/WorktreeCli/BuildCommand.cpp` — target argument parsing, comparable
  identity, pre-MSBuild existence check, and result projection (`:496-504`,
  `:807-839`).

## In scope

- Root-cause investigation via `/next-plan-review` with the recorded
  provenance.
- The smallest resulting fix confined to the documented full-build target
  invocations and structured-result guidance in
  `.agents/skills/compile/SKILL.md`, and the target path handling/result
  production in `Tools/WorktreeCli/BuildCommand.cpp`.

## Out of scope

- The landed UiState/GameBase change and every file named by
  `Documents/Plans/Engine/UiStateHoistToGameBase.md`.
- The distinct post-build `target.normalized` mismatch owned by
  `Documents/Plans/Agents/WorktreeCliBuildTargetNormalizedIdentity.md`; this
  Plan must preserve that Plan's result-identity acceptance surface.
- Runtime data generation, `.pack`/`.manifest` contents, DataPacker behavior,
  unrelated WorktreeCli verbs, other skills/scripts, and any transcript path or
  transcript text in the repository.

## Risk tier and invariants

Expected Tier 3 if the smallest fix changes `Tools/WorktreeCli`: the shared
AgentTools rebuild and promotion path is build/bootstrap coordination that can
block other sessions. If `/next-plan-review` proves the fix is documentation
only, reclassify from the actual changed surface. Preserve native Windows
argument boundaries, the documented client/server targets, the exact
`broken-engine-build-result/v1` status and failure semantics, and the rule that
MSBuild is not reported as launched when target validation fails. This tooling
surface does not expose deterministic simulation, CRC, replay, wire,
serialization, shader, or runtime allocation state. Never embed transcript
paths or home paths.

## Coordination

- `Documents/Plans/Agents/WorktreeCliBuildTargetNormalizedIdentity.md` records
  a different symptom: a successful build whose normalized result names the
  wrong solution. The two Plans are order-independent but can touch the same
  `BuildCommand.cpp` target/result region and compile-skill target contract;
  whichever lands second must re-read the first Plan's changed region and
  preserve both the pre-MSBuild existence behavior and the normalized-result
  contract.
- `Documents/Plans/Agents/CompileSharedDataVersionDrift.md` records the
  separate Shared runtime-data/source-version mismatch. It shares only the
  compile skill's workflow guidance; whichever lands second must re-read those
  sections and preserve both the target invocation contract and the
  data-mode/oracle rules.

## Acceptance criteria

- The documented absolute-target Debug client and server invocations no longer
  reproduce the `status:fail`, `failureKind:tool`, `build target does not exist`,
  `msbuild.launched:false` result when the referenced solutions exist, and no
  repository-relative replacement invocation is required.
- A genuine missing target still fails before MSBuild with an explicit tool
  failure, while an existing target reaches MSBuild and reports its actual
  result through `broken-engine-build-result/v1`.
- `/next-plan-review` records the proven root cause and smallest fix within the
  named skill/WorktreeCli boundary; if the root cause is outside it, the issue
  is surfaced for re-planning instead of expanding scope.
- `/validate-skill` passes if `.agents/skills/compile/SKILL.md` changes, and
  WorktreeCli `plan validate` exits `0` with `status:valid` and `code:ok`.

## Notes

This Plan is keyed to the concrete compile-skill/WorktreeCli target symptom:
the documented absolute solution target returns a typed tool failure saying
`build target does not exist`, while the equivalent repository-relative target
builds both executables. A later observation of this same skill/observed
symptom pair is a duplicate, not a new residual. The root cause is intentionally
deferred to `/next-plan-review`; this body records the command, result,
workaround, and provenance without embedding transcript material.
