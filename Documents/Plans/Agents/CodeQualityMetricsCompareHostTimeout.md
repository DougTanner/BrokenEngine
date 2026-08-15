<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-15T22:36:10.730Z","dependsOn":[]} -->
# Fix: Invoke-CodeQualityMetrics.ps1 — Compare exceeds the ordinary host call timeout

## Context

During C++ review preparation for the claimed
`Documents/Plans/DataPacker/FileManagerWorktreeDecomposition.md` Plan, the
documented Compare command was run (the `<worktree-root>` placeholder keeps the
session's absolute home path out of tracked content):

```text
pwsh -NoProfile -File .agents/skills/code-quality-metrics/scripts/Invoke-CodeQualityMetrics.ps1 -Mode Compare -Targets Temp/FileManagerWorktreeDecomposition-review-targets.json -Baseline a92acd1fd1bd3fb2c6eb41055de4f92e859640fd -RepositoryRoot <worktree-root> -Digest
```

The first foreground call was killed by the host after `124.029` seconds with
exit `124` and no digest. The identical command was repeated with a longer
timeout and completed successfully in `129.2` seconds. The forced retry was
the only workaround; without it, review preparation had no metrics digest.

The public Compare invocation is documented at
`.agents/skills/code-quality-metrics/SKILL.md:31-37`. The entry script holds a
bootstrap mutex for up to ten minutes at
`.agents/skills/code-quality-metrics/scripts/Invoke-CodeQualityMetrics.ps1:326`
and waits synchronously for the analyzer at `:385-399`, so the documented
command can exceed an ordinary host call cap even though it eventually
completes. The exact root cause is intentionally deferred to
`/next-plan-review`.

The active Plan's `## In scope` is limited to behavior-preserving extraction in
`FileManager::InitializeWorktreeOutputs` and `FileManager::MaterializeOutput`
(`Documents/Plans/DataPacker/FileManagerWorktreeDecomposition.md:27-34`). The
metrics script and its skill are outside that boundary. This is therefore
review-preparation tooling friction, not an in-scope FileManager acceptance
failure.

Session provenance (machine-local; not reproducible after cleanup):
- Client: codex
- Conversation session ID: none
- Worktree/branch UUID: 580d2d34-6de9-4fde-a836-02026d3430dd
- Session branch: codex/580d2d34-6de9-4fde-a836-02026d3430dd
- Worktree: .codex\worktrees\BrokenEngine\580d2d34-6de9-4fde-a836-02026d3430dd
- Landing ref: the session branch above, whose tip is the session's final
  commit and which survives exactly as long as the worktree recorded above.
  Fallback once that branch is gone:
  `git log --diff-filter=A --format=%H -- <this plan path>`, but a periodic
  Plan-history squash can make it return an unrelated aggregate commit, so
  review its result only when the commit is attributable to this session alone
  (its diff limited to this session's files); never review an aggregate or
  multi-session squash commit.
- Run the review before /cleanup-worktrees removes this worktree: Codex
  transcript discovery requires the producing worktree to remain registered.

## Design

In a new session, run `/next-plan-review <landing ref>` supplying the recorded
Codex client and landing ref. Root-cause the timeout friction from the proven
session evidence, then make the smallest fix inside the `## In scope`
boundary below. The successful Compare result must remain a complete,
schema-valid digest bound to the supplied targets file and baseline; genuine
input, bootstrap, analyzer, and persistence failures must retain meaningful
failure reporting. If root-causing shows the fix reaches shared
build/bootstrap coordination or lies outside the named files, surface it for
re-planning instead of expanding scope.

## Critical files

- `.agents/skills/code-quality-metrics/scripts/Invoke-CodeQualityMetrics.ps1` —
  Compare bootstrap and analyzer wait path
- `.agents/skills/code-quality-metrics/SKILL.md` — documented Compare
  invocation and completion contract

## In scope

- Root-cause investigation via `/next-plan-review`, run with the recorded
  landing ref and Codex client
- The smallest resulting fix confined to the Compare path in
  `.agents/skills/code-quality-metrics/scripts/Invoke-CodeQualityMetrics.ps1`
  and its documented invocation/completion guidance in
  `.agents/skills/code-quality-metrics/SKILL.md`

## Out of scope

- The landed FileManager extraction and every file named by
  `Documents/Plans/DataPacker/FileManagerWorktreeDecomposition.md`
- The metrics schema, digest fields, target-selection/baseline contract, and
  analyzer scoring or thresholds
- The distinct `codex-review` host-cap wrapper symptom recorded by
  `Documents/Plans/Agents/CodexReviewLongRunHostTimeout.md`
- The distinct targets/digest ordering symptom recorded by
  `Documents/Plans/Agents/CodexReviewMetricsTargetsCircularInput.md`
- Unrelated skills/scripts; any transcript path or transcript text in the
  repository

## Risk tier and invariants

Expected Tier 2 (scoped metrics-tool behavior); escalate if the fix reaches
shared AgentTools build/bootstrap coordination. A successful Compare must keep
the existing target-file and full-SHA baseline binding, emit the complete
`broken-engine-code-quality-evidence/v2` digest, and preserve the script's
meaningful failure/exit behavior. This tool does not expose deterministic
simulation, CRC, replay, wire, serialization, shader, or runtime allocation
state. Never embed transcript paths or home paths.

## Acceptance criteria

- The documented Compare invocation completes without host termination or a
  forced retry; root-cause resolution remains deferred to `/next-plan-review`.
- Its successful output is a complete, schema-valid digest whose target
  selection remains bound to `Temp/FileManagerWorktreeDecomposition-review-targets.json`
  and whose comparison remains bound to the recorded full baseline SHA.
- Genuine Compare failures remain explicit and do not produce a misleading
  partial or stale digest.
- `/validate-skill` passes for `.agents/skills/code-quality-metrics/SKILL.md` if
  it changes; WorktreeCli `plan validate` exits `0` with `status: valid` and
  `code: ok`.

## Notes

This Plan is keyed to the pair (`Invoke-CodeQualityMetrics.ps1` Compare,
ordinary host call kills the foreground invocation around 124 seconds before a
digest while a longer-timeout retry completes around 129 seconds). A later
observation of the same pair is a duplicate, not a new residual. It is
distinct from `CodexReviewLongRunHostTimeout.md` (a different wrapper and
ten-minute cap) and `CodexReviewMetricsTargetsCircularInput.md` (a different
targets/digest ordering failure).
