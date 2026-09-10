<!-- broken-engine-plan/v1 {"createdUtc":"2026-09-10T17:32:47.528Z","dependsOn":[]} -->
# Fix: /compile — the handoff re-renders data-mode facts the brief already fixed and the envelope already holds

## Context
Observed during a `/next-plan` run on an engine C++ Plan, across both `builder`
`/compile` dispatches it made.

`.agents/skills/compile/SKILL.md:85-91` (`## Handoff`) requires every game build
to report `DataBuildMode`, the `RunDataPacker` value for every build, normalized
`GameDataDirectory`, normalized `GeneratedDataIncludeRoot`, every mode-selection
trigger, the Local generation-authorization trigger, and the Gaea guard outcome.

Observed symptom. Both handoffs from that run duly re-rendered all of those
fields, plus elapsed times and absolute path lists, into the main session. Every
one of those values had already been fixed by the dispatch brief main wrote, and
the same handoffs cited the envelope file
`Temp/AgentBuildEnvelopes/compile-*.md`, whose `## Compile dispatch` section
holds the same values verbatim for whichever worker main dispatches next. Nothing
in either handoff's re-rendered block changed main's next action.

Cost. Main context consumed by material it had itself supplied and could re-read
from the cited envelope; the character volume was not measured in the observing
session, so this Plan records the redundancy, not a size.

Session provenance (machine-local; not reproducible after cleanup). The Client
through Worktree fields name the session that observed the friction — the session
`/next-plan-review` must reach — while the `Landing ref` line names a ref whose
tree actually contains this Plan:
- Client: claude
- Conversation session ID: a06b85a1-692a-49b9-917b-1565c31365f4
- Worktree/branch UUID: 31d26f5f-0202-44d3-a438-1ca3857ffb79
- Session branch: claude/31d26f5f-0202-44d3-a438-1ca3857ffb79
- Worktree: .claude\worktrees\BrokenEngine\31d26f5f-0202-44d3-a438-1ca3857ffb79
- Landing ref: the session branch above, whose tip is that session's final commit
  and which survives exactly as long as the worktree recorded above.
  Fallback once the recorded ref is gone:
  `git log --follow --diff-filter=A --format=%H -- Documents/Plans/ChangeWorkflow/CompileHandoffBoundToBrief.md`,
  but a periodic Plan-history squash can make it return an unrelated aggregate
  commit, so review its result only when the commit is attributable to one
  session alone (its diff limited to that session's files); never review an
  aggregate or multi-session squash commit.
- Run the review before /cleanup-worktrees removes the worktree recorded above:
  Codex transcript discovery requires the producing worktree to remain
  registered, and Claude review requires the exact conversation session ID above.

## Design
First root-cause the friction from the current tree and this Plan's `## Context`.
Only when the transcript is genuinely needed, in a new session run
`/next-plan-review <review ref>` in bounded friction mode — the landing ref above
— supplying the recorded client and the recorded conversation session ID. Then
make the smallest fix inside the `## In scope` boundary below. If root-causing
shows the fix lies outside that boundary, surface it for re-planning instead of
expanding scope.

The author's recommendation is to bound the handoff to the existing shared
`Decisive checks` and `Evidence` rows the section already narrows, and to carry a
data-mode line only when the resolved mode differs from the fixed decision the
dispatch brief stated — so a divergence from the brief still reaches main
immediately, while agreement costs one line or none. The fix session should
confirm from `.agents/skills/compile/references/` and the envelope writer that
the envelope really does hold each dropped field verbatim under
`## Compile dispatch`, and should keep any field that has no envelope home. The
worker-side reporting rules that mirror this section must be kept parallel with
whatever is decided.

## Critical files
- `.agents/skills/compile/SKILL.md` (`## Handoff`, the game-build reporting
  bullet and the narrowed shared-field paragraph)
- `.agents/skills/compile/references/worker.md` — only where its result
  discipline restates the same reporting requirement

## In scope
- Root-cause investigation as `## Design` states
- The smallest resulting fix, confined to the `## Handoff` section of
  `.agents/skills/compile/SKILL.md` and the mirrored reporting rules in
  `.agents/skills/compile/references/worker.md`

## Out of scope
- The landed change the session produced
- What the build envelope records and the envelope file's own format
- Build invocation, data-mode selection, the Gaea guard, and generation
  authorization behavior themselves — only how they are reported changes
- The shared handoff form in `.agents/references/subagent-handoff.md`
- Unrelated skills/scripts; any transcript path or transcript text in the repo

## Risk tier and invariants
Expected Tier 2: scoped tool behavior, because the fix changes what a `/compile`
dispatch reports to its caller. Escalate if the fix reaches build/bootstrap
coordination or the envelope contract other skills consume. A divergence between
the brief's fixed data mode and the resolved mode must still surface in the
handoff. Never embed transcript paths or home paths.

## Acceptance criteria
- A `/compile` handoff whose resolved data mode matches the brief no longer
  re-renders `DataBuildMode`, `RunDataPacker`, `GameDataDirectory`,
  `GeneratedDataIncludeRoot`, trigger analysis, elapsed times, or absolute path
  lists that the brief fixed and the cited envelope holds
- A resolved data mode differing from the brief's fixed decision is still
  reported in the handoff
- Failures, `severity: error` diagnostics, and retained-log paths are still
  reported inline
- /external-skill-creator validate mode passes wherever the Change Workflow Apply
  the triggered cleanup step triggers it; plan validate exits 0
