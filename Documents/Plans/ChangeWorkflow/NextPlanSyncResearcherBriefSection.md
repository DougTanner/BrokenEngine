<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-24T01:08:59.187Z","dependsOn":[]} -->
# Fix: /next-plan step 6 — main reads the whole sync-object section only to forward the sync researcher brief

## Context
Observed symptom: at the `/next-plan` final claim refresh, the claim result
carried a `sync` object. `.agents/skills/next-plan/SKILL.md` step 6 sends main
to dispatch one `researcher` "on the sync brief in" `claim-results.md`
`## Session baseline and the sync object`. Under the bounded-reading rule, a
citation that names a `##` section is the whole reading scope, so main read
that entire section (about 5 KB) to write one brief. Only two parts of it serve
main at that point: the baseline rule (its first paragraph) and the paragraph
that says what the sync brief carries (`sync.from..sync.to`, the preparation
snapshot's path and `##` selectors, and the preparation handoff rows citing
paths outside that snapshot). The rest of the section covers the claim
script's `reused`, refusal, fast-forward, divergence, and `integrity` results,
which step 6 does not need. It also holds the researcher's own steps: the
`git diff --name-only --no-renames` listing, the citation-matching rules, and
the `Decisive checks` row rules. Main only passed those on to the worker.

Cost: the run checkpoint's context-efficiency pass found that the main session
read the whole section only to forward it, when the researcher could have read
its own steps.

Session provenance (machine-local; not reproducible after cleanup). The Client
through Worktree fields name the session that observed the friction — the
session `/next-plan-review` must reach — while the `Landing ref` line names a
ref whose tree actually contains this Plan:
- Client: claude
- Conversation session ID: 4422f9e3-bfee-4a1a-abe2-268f45560966
- Worktree/branch UUID: b719c635-512a-4a63-bc40-8e90d6b34190
- Session branch: claude/b719c635-512a-4a63-bc40-8e90d6b34190
- Worktree: .claude\worktrees\BrokenEngine\b719c635-512a-4a63-bc40-8e90d6b34190
- Landing ref: claude/b719c635-512a-4a63-bc40-8e90d6b34190
  Fallback once the recorded ref is gone:
  `git log --follow --diff-filter=A --format=%H -- Documents/Plans/ChangeWorkflow/NextPlanSyncResearcherBriefSection.md`, but a periodic
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
`/next-plan-review <review ref>` in bounded friction mode — the landing ref —
supplying the recorded client and the recorded conversation session ID. Then
make the smallest fix inside the `## In scope` boundary below. If root-causing
shows the fix lies outside that boundary, surface it for re-planning instead of
expanding scope.

The author's recommended fix moves text without rewording it:
1. Move the final-refresh paragraph of
   `## Session baseline and the sync object` (the one that starts "When the
   final claim refresh ...") out into two new `##` sections in
   `claim-results.md`, placed right after that section:
   - `## Final-refresh sync brief` — main-facing: what main's one `researcher`
     brief carries (the `sync.from..sync.to` range, the preparation snapshot's
     path and `##` selectors, and the preparation handoff's one-line rows citing
     paths outside that snapshot). It adds that the brief cites
     `## Sync researcher steps` by path plus selector.
   - `## Sync researcher steps` — worker-facing: the `git diff` listing, the
     citation-matching rules, and the `Decisive checks` count and touch row
     rules, worded as today.
2. Point `.agents/skills/next-plan/SKILL.md` step 6 at
   `## Final-refresh sync brief` instead of
   `## Session baseline and the sync object`.

`## Session baseline and the sync object` keeps its heading, so its existing
citations stay valid: step 8 of `SKILL.md`, and
`.agents/references/subagent-reporting.md` `## Task brief` by heading and by the
`#session-baseline-and-the-sync-object` anchor. The baseline rule stays where
it is and is not restated in the new sections.

## Critical files
- `.agents/skills/next-plan/references/claim-results.md` —
  `## Session baseline and the sync object` and the two new sections
- `.agents/skills/next-plan/SKILL.md` — step 6 of `## Steps`

## In scope
- Root-cause investigation as `## Design` states
- The smallest resulting fix, confined to the final-refresh paragraph of
  `claim-results.md` `## Session baseline and the sync object`, the two new
  sections `## Design` names, and the step 6 citation in
  `.agents/skills/next-plan/SKILL.md`

## Out of scope
- The landed change the session produced
- The other paragraphs of `## Session baseline and the sync object` (the
  baseline rule and the claim script's `reused`, refusal, fast-forward,
  divergence, and `integrity` results)
- Behavior of `Invoke-NextPlanClaim.ps1` and the `sync` object it returns
- Unrelated skills/scripts; any transcript path or transcript text in the repo

## Risk tier and invariants
Expected Tier 1 (documentation: a meaning-preserving move of skill reference
prose and one citation, with no script or behavior change); escalate if the
fix reaches a script or build/bootstrap coordination. Every existing citation
of `## Session baseline and the sync object` must still resolve. Never embed
transcript paths or home paths.

## Acceptance criteria
- `SKILL.md` step 6 cites a `##` section that holds only the sync brief's
  contents, not the researcher's steps or the claim-result paragraphs
- The researcher's steps sit in their own `##` section, and the sync brief
  section tells main to cite it by path plus selector
- The recorded symptom no longer reproduces under the documented invocation:
  following step 6 as written, main reads only the brief section to write the
  sync `researcher` brief
- The static-checks runner, invoked as `.agents/references/change-workflow.md`
  `#### Step 5 — Run targeted pre-review checks` documents it, reports every
  row the change triggers passing
