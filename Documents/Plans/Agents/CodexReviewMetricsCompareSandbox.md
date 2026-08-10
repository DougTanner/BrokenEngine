<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-09T13:58:39.901Z","dependsOn":[]} -->
# Fix: codex-review / repo-code-review — metrics Compare blocked by the read-only sandbox

## Context

During this session's Change Workflow Step 5, the delegated `/repo-code-review` ran on Codex through
`/codex-review`, whose Codex invocation uses `--sandbox read-only`
(`.agents/skills/codex-review/SKILL.md`). `repo-code-review`'s `## Workflow` step 1
(`.agents/skills/repo-code-review/SKILL.md:64-71`) mandates a `code-quality-metrics` Compare run:

```
pwsh -NoProfile -File .agents/skills/code-quality-metrics/scripts/Invoke-CodeQualityMetrics.ps1 -Mode Compare -Targets <supplied-targets-file> -Baseline <fixed-full-sha> -RepositoryRoot <absolute-checkout-root> -Digest
```

That run needs to write the `Temp/CodeQualityMetrics` cache, which
`.agents/skills/repo-code-review/references/metrics-protocol.md:18-20` explicitly permits as the one allowed
write. The read-only sandbox denied that write, so the Compare could not complete and the reviewer returned
`BLOCKED: mandatory code-quality metrics cache write denied` with no review content. The manager then re-ran
the same Compare host-side, where it passed, and had to reconcile that result back into the review by hand —
one wasted reviewer dispatch plus a manual rework step.

`/codex-review` already has a documented mechanism for exactly this shape of problem:
`.agents/skills/codex-review/SKILL.md:59-64` tells the caller to run a mechanical, non-judgment tool check
host-side first and embed its verbatim result in `-ScopeFile` when the read-only sandbox cannot run it. The
metrics Compare is such a check but is not covered by that path, and `repo-code-review` states no sandbox
fallback of its own, so the two contracts disagree at the point where they meet.

The failing skills are outside the claimed Plan's `## In scope`
(`Documents/Plans/Network/ClientLoadResetEpochBarrier.md`), which covers only client network C++, the project
agent fixture, and AgentHarness/AGENTS documentation.

Session provenance (machine-local; not reproducible after cleanup):

- Client: claude
- Session: 54c5a880-1579-4009-8eb5-24dccfa6d10e
- Session branch: claude/54c5a880-1579-4009-8eb5-24dccfa6d10e
- Worktree: .claude\worktrees\BrokenEnginePublic\54c5a880-1579-4009-8eb5-24dccfa6d10e
- Landing commit: `git log --diff-filter=A --format=%H -- Documents/Plans/Agents/CodexReviewMetricsCompareSandbox.md`
- Run the review before /cleanup-worktrees removes this worktree: Codex transcript discovery requires the
  producing worktree to remain registered, and Claude review requires the exact session id above.

## Design

In a new session, run `/next-plan-review <landing commit>` supplying the recorded client and session id,
root-cause the friction from the proven transcript, then make the smallest fix inside the `## In scope`
boundary below. If root-causing shows the fix lies outside that boundary, surface it for re-planning instead
of expanding scope.

Two candidate shapes are already visible from the symptom, and root-causing decides between them: either
`New-CodexReviewPrompt.ps1` pre-runs the metrics Compare host-side for the `repo-code-review` assigned skill
and embeds the verbatim digest in the prompt, the way it already writes the targets file for that same skill;
or `repo-code-review`'s step 1 and `metrics-protocol.md` state the read-only-sandbox fallback explicitly so
the reviewer proceeds on an embedded result instead of returning `BLOCKED`.

## Critical files

- `.agents/skills/codex-review/SKILL.md`
- `.agents/skills/codex-review/scripts/New-CodexReviewPrompt.ps1`
- `.agents/skills/codex-review/references/prompt-template.md`
- `.agents/skills/repo-code-review/SKILL.md`
- `.agents/skills/repo-code-review/references/metrics-protocol.md`

## In scope

- Root-cause investigation via `/next-plan-review` with the recorded provenance.
- The smallest resulting fix, confined to the files named above — specifically the `/codex-review` prompt
  assembly and its documented host-side mechanical-check path, and `repo-code-review`'s `## Workflow` step 1
  plus its metrics protocol reference.

## Out of scope

- The landed change the session produced.
- `.agents/skills/code-quality-metrics/` and its analyzer or cache behavior.
- Unrelated skills and scripts; any transcript path or transcript text in the repository.

## Risk tier and invariants

Expected Tier 2 (scoped tool behavior); escalate if the fix reaches build/bootstrap coordination. Never embed
transcript paths or home paths. The metrics result must stay advisory and must not become a review finding.

## Acceptance criteria

- The recorded symptom no longer reproduces: a `/repo-code-review` dispatched through `/codex-review` in the
  read-only sandbox completes its mandated Compare step without a cache-write denial and without the manager
  re-running it host-side.
- `/validate-skill` passes for any changed `SKILL.md`; `plan validate` exits 0.
