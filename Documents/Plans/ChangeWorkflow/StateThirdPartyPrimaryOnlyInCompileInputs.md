<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-13T15:57:46.719Z","dependsOn":[]} -->
# Fix: /compile — the dispatcher contract never says ThirdParty builds run only in the primary checkout

## Context
Observed symptom: during a `/next-plan` run of `Documents/Plans/Engine/TextChat.md`, main dispatched a `/compile` builder from a session worktree requesting client, server, and ThirdParty Debug|x64. The claimed Plan's acceptance criterion 9 required that ThirdParty build, because the change edited only a comment in `ThirdParty/Prebuilts/Source/Engine/DirectXTK.cpp`. The ThirdParty invoke was blocked with code `thirdparty.worktree-not-primary`, exitCode 2. That block comes from `Stop-CompileInvoke` at `.agents/skills/compile/scripts/Invoke-CompileBuild.ps1:257`. The client and server builds succeeded.

`.agents/skills/compile/SKILL.md` is the contract a dispatcher reads, and its `## Inputs` covers targets and configurations (:38-43). It never says that ThirdParty builds run only in the primary checkout. That rule appears only in the executor's `.agents/skills/compile/references/worker.md:65-73`, which cites `ThirdParty/Prebuilts/Platforms/VisualStudio2026/AGENTS.md` `## Consumer Provisioning`. Neither the dispatcher nor the Plan author could see the rule. So the Plan carried a criterion that no worktree session can satisfy through `/compile`, and the build request hit the block.

Rework it forced: the requested ThirdParty build step was skipped. The ThirdParty part of criterion 9 was settled by reading instead, because the edit was comment-only.

Session provenance (machine-local; not reproducible after cleanup). The Client
through Worktree fields name the session that observed the friction — the
session `/next-plan-review` must reach — while the `Landing ref` line names a
ref whose tree actually contains this Plan:
- Client: claude
- Conversation session ID: 991ce52f-73b4-4395-9681-17d7a8f99e0c
- Worktree/branch UUID: a5dbdc84-b603-412d-b721-e183ad394573
- Session branch: claude/a5dbdc84-b603-412d-b721-e183ad394573
- Worktree: .claude\worktrees\BrokenEngine\a5dbdc84-b603-412d-b721-e183ad394573
- Landing ref: claude/a5dbdc84-b603-412d-b721-e183ad394573 — the observing session records and lands this Plan itself; that branch survives exactly as long as the worktree above.
  Fallback once the recorded ref is gone:
  `git log --follow --diff-filter=A --format=%H -- Documents/Plans/ChangeWorkflow/StateThirdPartyPrimaryOnlyInCompileInputs.md`, but a periodic
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
`/next-plan-review <review ref>` in bounded friction mode with the landing ref,
supplying client `claude` and the conversation session ID above. Then make the
smallest fix inside the `## In scope` boundary below. If root-causing shows the
fix lies outside that boundary, surface it for re-planning instead of expanding
scope.

The author recommends stating the primary-only rule once in `/compile`
`## Inputs`, where a dispatcher reads targets and a Plan author can find it.
That statement should reference `ThirdParty/Prebuilts/Platforms/VisualStudio2026/AGENTS.md`
`## Consumer Provisioning` rather than restate its mechanics. Rationale: the
rule is already owned and enforced; the gap is only that the dispatcher-facing
contract omits it. The fix session also decides from evidence whether
`## Inputs` must name how a worktree session verifies a ThirdParty-only change,
such as a comment-only wrapper edit. One such route is the reading-based
settlement the observed run used. Do not add any new build path.

## Critical files
- `.agents/skills/compile/SKILL.md`

## In scope
- Root-cause investigation as `## Design` states
- The smallest resulting fix, confined to `.agents/skills/compile/SKILL.md`
  `## Inputs` (the targets-and-configurations bullet)

## Out of scope
- The landed TextChat change and `Documents/Plans/Engine/TextChat.md`
- `Invoke-CompileBuild.ps1` behavior, the `thirdparty.worktree-not-primary`
  block itself, and ThirdParty provisioning rules
- `.agents/skills/compile/references/worker.md`, except where the fix would
  leave its wording contradicting `## Inputs`
- Unrelated skills/scripts; any transcript path or transcript text in the repo

## Risk tier and invariants
Expected Tier 1. Trigger: documentation only, a skill contract statement with
no script or invariant change. Escalate to Tier 2 if the fix changes `/compile`
tool behavior. Re-plan if it reaches build/bootstrap coordination. Never embed
transcript paths or home paths.

## Acceptance criteria
- A dispatcher reading only `/compile` `SKILL.md` `## Inputs` learns that
  `-Target ThirdParty` is blocked outside the primary checkout, so the recorded
  symptom no longer reproduces under the documented invocation
- The primary-only rule keeps one owner; `## Inputs` references it instead of
  restating its mechanics
- The static-checks runner, invoked as `.agents/references/change-workflow.md`
  `#### Step 5 — Run targeted pre-review checks` documents it, reports every
  row the change triggers passing
