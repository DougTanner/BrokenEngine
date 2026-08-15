<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-15T17:46:38.320Z","dependsOn":[]} -->
# Fix: finalize-changes fixture suites — no documented invocation anywhere in the repository

## Context
Implementing `Documents/Plans/Agents/FinalizeLandingSanityTransientPrimaryDirty.md`
required running the finalize workflow's scratch-repository fixture suite
`.agents/skills/finalize-changes/scripts/Test-FinalizeWorkflowFixtures.ps1`,
because that Plan's acceptance criteria are landing-sanity-gate behaviors the
suite covers. No documented invocation for it exists: a repository-wide search
for `Test-FinalizeWorkflowFixtures` returns zero hits in any file — no
`SKILL.md`, no reference, no `AGENTS.md`, not even the script itself. The
finalize skill's bundled-script reference
`.agents/skills/finalize-changes/references/scripts.md:15-24` lists exact
invocation lines for eight bundled scripts (`Invoke-FinalizeCandidateCommit`,
`Invoke-FinalizeApprovalPreparation`, `Invoke-FinalizeLockClaim`,
`Invoke-FinalizeLanding`, `Show-FinalizeApprovalReview`,
`Wait-AgentToolsQuiescence`, `Invoke-AgentToolsPromotion`) and omits every
fixture suite.

Root `AGENTS.md`'s "Bundled scripts as documented" directive requires running a
bundled script exactly as its skill documents it, so with no documentation the
session had to infer the canonical form by reading the script's own `param`
block, then run:

```text
pwsh -NoProfile -File .agents/skills/finalize-changes/scripts/Test-FinalizeWorkflowFixtures.ps1 -WorktreeCliExecutable <provisioned WorktreeCli path>
```

The two sibling fixture suites in the same directory share the gap and were
checked in the same search: `Test-LandingLockStatusFixtures.ps1` (mandatory
`-WorktreeCliExecutable`) and `Test-AgentToolsPromotionFixtures.ps1` (mandatory
`-WorktreeCliExecutable` *and* `-AgentHarnessExecutable`) are likewise named
nowhere in the repository, so neither their arguments nor the file changes that
should trigger a run are discoverable without reading the scripts.

The comparable next-plan suite is documented:
`.agents/skills/next-plan/SKILL.md:136-145` gives the exact invocation of
`Test-NextPlanWorkflowScripts.ps1`, the list of files whose change requires the
run, and where the substituted executable path comes from.

The claimed active intent was
`Documents/Plans/Agents/FinalizeLandingSanityTransientPrimaryDirty.md`, whose
`## In scope` covers only a bounded retry in the finalize sanity gate plus the
matching landing-retry documentation; the fixture suites and their invocation
documentation are outside that boundary.

Session provenance (machine-local; not reproducible after cleanup):
- Client: claude
- Conversation session ID: e6751460-6c46-40f2-8f6a-f6f3d125783e
- Worktree/branch UUID: c36fe1de-7d25-41df-b222-5f448e8be87f
- Session branch: claude/c36fe1de-7d25-41df-b222-5f448e8be87f
- Worktree: .claude\worktrees\BrokenEngine\c36fe1de-7d25-41df-b222-5f448e8be87f
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
client and, on Claude, the recorded conversation session ID, to confirm the
recorded symptom from the session's own evidence and root-cause the friction.
Then make the smallest fix inside the `## In scope` boundary below: document
each of the three finalize fixture suites the way
`.agents/skills/next-plan/SKILL.md` documents `Test-NextPlanWorkflowScripts.ps1`
— for each suite, the exact canonical invocation with its mandatory parameters,
the list of bundled files whose change requires that run, and where the
substituted executable path comes from. If root-causing shows the fix lies
outside that boundary, surface it for re-planning instead of expanding scope.

## Critical files
- `.agents/skills/finalize-changes/references/scripts.md` — the bundled-script
  invocation list (`:9-30`) that omits every fixture suite.
- `.agents/skills/finalize-changes/SKILL.md` — the finalize skill's bundled-script
  section, which decides whether the fixture documentation lives here or in the
  reference above.
- `.agents/skills/finalize-changes/scripts/Test-FinalizeWorkflowFixtures.ps1` —
  the undocumented suite and its mandatory `-WorktreeCliExecutable` parameter.
- `.agents/skills/finalize-changes/scripts/Test-LandingLockStatusFixtures.ps1` —
  sibling suite sharing the gap.
- `.agents/skills/finalize-changes/scripts/Test-AgentToolsPromotionFixtures.ps1` —
  sibling suite sharing the gap, with two mandatory executable parameters.

## In scope
- Symptom confirmation and root-cause investigation via /next-plan-review, run
  with the recorded landing ref and client, plus, on Claude, the recorded
  conversation session ID; a Codex review supplies the client and landing ref
  only
- The smallest resulting fix, confined to the finalize skill's bundled-script
  documentation in the two documentation files named above: add the canonical
  invocation and the change-trigger file list for the three fixture suites named
  above

## Out of scope
- The landed change the session produced
- Any behavior change to the fixture suites or to the finalize bundled scripts
  they exercise; this is a documentation gap only
- The next-plan skill's own fixture documentation, which is the model to mirror
  rather than something to change
- Unrelated skills/scripts; any transcript path or transcript text in the repo

## Risk tier and invariants
Expected Tier 1 (documentation only, no public signature or invariant
exposure); escalate to Tier 2 if root-causing shows a script parameter or
result contract must change too. The documented form must match root
`AGENTS.md`'s canonical bundled-script invocation rule exactly — repo-relative
script path, `pwsh -NoProfile -File`, one invocation per shell call, no
`-ExecutionPolicy Bypass`. Never embed transcript paths or home paths.

## Acceptance criteria
- Each of the three fixture suites has a documented invocation that binds its
  mandatory parameters and runs successfully as written, together with the list
  of bundled files whose change requires the run
- A repository-wide search for each suite's script name returns at least one
  documentation hit
- /validate-skill passes for any changed SKILL.md; plan validate exits 0

## Notes
This Plan is keyed to the pair (finalize-changes fixture suites, no documented
invocation anywhere in the repository). A later observation of the same pair is
a duplicate, not a new residual.
`Documents/Plans/Agents/FinalizeApprovalReviewInvocationContract.md` is a
different pair — a documented invocation of `Show-FinalizeApprovalReview.ps1`
that omits mandatory parameters — and is not this Plan's symptom.
