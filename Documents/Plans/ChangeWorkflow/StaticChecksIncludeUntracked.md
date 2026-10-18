<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-18T15:35:48.933Z","dependsOn":[]} -->
# Fix: Change Workflow Step 5 static checks — documented command skips untracked new files and still passes

## Context
Observed symptom: during a `/next-plan` run, main ran the Change Workflow
pre-review static checks exactly as `.agents/references/change-workflow.md:185`
(`#### Step 5 — Run targeted pre-review checks`) documents them:
`pwsh -NoProfile -File .agents/scripts/Invoke-StaticChecks.ps1 -RepositoryRoot '<worktree root>' -Baseline <baseline SHA>`.
The session had added a whole new, still untracked skill package under
`.agents/skills/<name>/` (seven files). The runner returned `status pass` with
the `validate-skill` row skipped and the `file-hygiene` row covering only the
two changed tracked files; its result did not mention the untracked files at
all. Main noticed only by running `git status`, read the runner header, and
reran with `-IncludeUntracked`, which then validated one package across seven
files. A session that does not notice passes static checks with every new file
unchecked.

Mechanism, from the current tree:
- `.agents/scripts/Invoke-StaticChecks.ps1:20` (header) and `:118-123`
  (`Get-InventoryDocument`) add the untracked paths to the inventory call only
  under the `-IncludeUntracked` switch; without it the inventory sees tracked
  and staged changes only.
- `.agents/scripts/Get-SessionChangeInventory.ps1:803-810` already counts the
  untracked files nobody listed (`counts.unlistedUntracked`), but the runner
  does not surface that count, so its pass is silent about them.
- No caller passes the switch: the Step 5 bullet above and the mirrored
  `.agents/skills/sweep/SKILL.md:129` (step 9) both document the same command
  without it, and no other skill or script invokes the runner. The switch has
  been absent from the documented Step 5 command since the runner moved into
  Step 5.
- `.agents/scripts/Find-SessionCandidates.ps1:14` and `:160-164` carry the same
  opt-in `-IncludeUntracked` switch that enumerates the whole untracked set, so
  the runner's opt-in shape mirrors an existing pattern rather than being a
  stray default.

Session provenance (machine-local; not reproducible after cleanup). The Client
through Worktree fields name the session that observed the friction — the
session `/next-plan-review` must reach — while the `Landing ref` line names a
ref whose tree actually contains this Plan:
- Client: claude
- Conversation session ID: 59ca7158-c0e1-4598-bd90-ff4c5883b649
- Worktree/branch UUID: e24ddd05-b80e-46c5-99e8-bcca5bf89686
- Session branch: claude/e24ddd05-b80e-46c5-99e8-bcca5bf89686
- Worktree: .claude\worktrees\BrokenEngine\e24ddd05-b80e-46c5-99e8-bcca5bf89686
- Landing ref: claude/e24ddd05-b80e-46c5-99e8-bcca5bf89686 (this observing
  session records and lands this Plan itself; the branch survives exactly as
  long as the worktree recorded above).
  Fallback once the recorded ref is gone:
  `git log --follow --diff-filter=A --format=%H -- Documents/Plans/ChangeWorkflow/StaticChecksIncludeUntracked.md`,
  but a periodic Plan-history squash can make it return an unrelated aggregate
  commit, so review its result only when the commit is attributable to one
  session alone (its diff limited to that session's files); never review an
  aggregate or multi-session squash commit.
- Run the review before /cleanup-worktrees removes the worktree recorded above:
  Claude review requires the exact conversation session ID above.

## Design
First root-cause the friction from the current tree and this Plan's `## Context`.
Only when the transcript is genuinely needed, in a new session run
`/next-plan-review <review ref>` in bounded friction mode with the landing ref
above, supplying client `claude` and the recorded conversation session ID. Then
make the smallest fix inside the `## In scope` boundary below. If root-causing
shows the fix lies outside that boundary, surface it for re-planning instead of
expanding scope.

The author recommends adding the `-IncludeUntracked` switch to both documented
runner invocations — the Step 5 bullet in `.agents/references/change-workflow.md`
and step 9 of `.agents/skills/sweep/SKILL.md` — and leaving
`Invoke-StaticChecks.ps1` unchanged. Rationale:
- The runner already does the right thing under the switch, and its header
  documents it; the defect is that the only documented commands omit it. Both
  invocations check the working tree after propagation, which is where a
  session's new files are still untracked.
- Keeping the switch opt-in keeps the runner parallel with
  `Find-SessionCandidates.ps1`, whose identical switch stays opt-in. Making
  inclusion the runner's default would diverge from that mirror, change the
  script and its header, and remove the caller's choice, for no gain over a
  one-token edit to the two commands that exist.
- Surfacing `counts.unlistedUntracked` in the runner result would still return
  `pass` and still require a rerun with the switch, so it reports the gap
  instead of closing it.
- The switch also picks up untracked files the session did not create, such as
  a file already untracked before the session began. The runner is
  report-only (its header: it never decides whether a failing check blocks a
  slice), so main judges such a row as it judges any other; no extra filtering
  is warranted.
- Under `-Head`, the inventory ignores untracked paths
  (`Get-SessionChangeInventory.ps1:805`), so the switch is harmless if a caller
  later adds `-Head` to either command.

## Critical files
- `.agents/references/change-workflow.md`
- `.agents/skills/sweep/SKILL.md`

## In scope
- Root-cause investigation as `## Design` states
- The smallest resulting fix, confined to the runner command in the
  `#### Step 5 — Run targeted pre-review checks` bullet of
  `.agents/references/change-workflow.md` and the step 9 command block of
  `.agents/skills/sweep/SKILL.md` `## Steps`

## Out of scope
- The landed change the session produced
- `.agents/scripts/Invoke-StaticChecks.ps1`, `Get-SessionChangeInventory.ps1`,
  and `Find-SessionCandidates.ps1` (behavior and headers unchanged)
- The runner command in `Documents/Features/Engine/UpdateThirdPartySubmodules.md`
  (a manual Feature document, not a scheduler input)
- Unrelated skills/scripts; any transcript path or transcript text in the repo

## Risk tier and invariants
Tier 1 (documentation: two documented command lines gain one switch; no script
behavior changes). Escalate to Tier 2 if root-causing moves the fix into
`Invoke-StaticChecks.ps1`. Never embed transcript paths or home paths.

## Acceptance criteria
- Both documented runner commands carry `-IncludeUntracked`, so in a worktree
  holding an untracked new skill package, running the Step 5 command as
  documented triggers `validate-skill` for that package and includes its files
  in the `file-hygiene` row
- The static-checks runner, invoked as `.agents/references/change-workflow.md`
  `#### Step 5 — Run targeted pre-review checks` documents it, reports every
  row the change triggers passing

## Notes
- No `dependsOn`: no other Plan owns the runner invocation or its untracked-file
  handling.
